#include "policy.h"

#include "ast.h"
#include "lexer.h"
#include "synan.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <list>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace std;

static map<string, shared_ptr<topology_t>> get_simple_topologies(
    const shared_ptr<ast_node_t> &ast
);
static map<string, shared_ptr<topology_t>> &add_expr_topologies(
    const shared_ptr<ast_node_t> &ast,
    map<string, shared_ptr<topology_t>> &topologies
);
static shared_ptr<topology_basic_t> construct_expr_topology(
    shared_ptr<ast_expr_t> &expr,
    map<string, shared_ptr<topology_t>> &topologies,
    shared_ptr<topology_basic_t> &arg
);

static vector<pg_t> get_pgs(
    const shared_ptr<ast_node_t> &ast, const topology_basic_t &topology
);

static inline string remove_space(const string &s);

static void topological_sort_dfs(
    const vector<vector<uint8_t>> &m,
    const int index,
    vector<bool> &discovered,
    vector<int> &end_time,
    int &time,
    list<int> &topological_order
);
static list<int> topological_ordering(const vector<vector<uint8_t>> &m);

/**
 * @param file_path Takes the file path to the policy file as a string. The file
 * then gets lexified and parsed, after which an AST tree is constructed.
 */
policy_t::policy_t(const char *file_path) {
    vector<symbol_t> symbols   = lexify(file_path);
    dertree_t tree             = parse_source(symbols);
    shared_ptr<ast_node_t> ast = ast_construct(tree, nullptr);

    topologies = get_simple_topologies(ast);
    topologies = add_expr_topologies(ast, topologies);
    topology   = make_shared<topology_basic_t>("Total");

    /* At this point, `topologies` contains {topology name}->{topology_t}
     * translations.
     * The following for loop goes over all these translations and
     * - inserts the current {topology name} into a set containing all topology
     *  names, first for basic and then for linear topologies.
     * - adds the current (basic or linear) topology to the current topology.
     */
    for (auto &tuple : topologies) {
        if (auto t = dynamic_pointer_cast<topology_basic_t>(tuple.second)) {
            for (auto &m : t->index_mapping()) {
                tags.insert(m.first);
            }
            topology->disjoint_union(topology, t);
        }
        if (auto t = dynamic_pointer_cast<topology_linear_t>(tuple.second)) {
            for (auto &s : t->get_tags()) {
                tags.insert(s);
            }
            // Converts linear topologies to basic
            auto converted = make_shared<topology_basic_t>(*t);
            topology->disjoint_union(topology, converted);
        }
    }

    tags.insert("unknown");
    topology->add_unknown();

    // check DAG (Directed Acyclic Graph)
    topological_ordering(topology->matrix());

    perimeter_guards = get_pgs(ast, *topology);

    /* TODO: At this point, topologies must also contain the downgrader tags.
     *
     * The policy.mtag that is the result of tag-parser.cc program contains a
     * matrix of LCAs (Least Common Ancestors), which has the shape of:
     *
     *      ...
     *      unknown 0 1 2
     *      PP.private 1 1 1
     *      PP.public 2 1 2
     *      output "stdout" 2
     *      ...
     *
     * One way to add the aware flags, is to make another matrix of booleans,
     * such that if a \——> b, then m[a][b] = 1, otherwise m[a][b] = 0
     *
     * When a retag command is called, the CPU must check `if m[a][b]`. If = 0,
     * it results in a hard-fault.
     *
     * TO RECAP (the unwritten too):
     * When coming to the aware declaration, the tag-parser.cc must first check
     * whether all tags in declrest are already defined. If not => error.
     * It then undergoes the same process as a normal topology
     * (`get_simple_topologies()` and `add_expr_topologies()`). This resoult
     * would normally get further processed into a LCA matrix, but here there we
     * must print the adjacency matrix to the policy.mtag
     */
}

/**
 * @brief goes over all simple topologies (basic and linear topologies) and
 * processes them.
 *
 * @param ast: the AST (abstract syntax tree) from ast_construct().
 */
static map<string, shared_ptr<topology_t>> get_simple_topologies(
    const shared_ptr<ast_node_t> &ast
) {
    map<string, shared_ptr<topology_t>> topologies;

    if (auto source = dynamic_pointer_cast<ast_source_t>(ast)) {
        for (auto &decl : source->get_decls()) {
            if (auto t = dynamic_pointer_cast<ast_topology_basic_t>(decl)) {
                // Check for double definition
                if (topologies.find(t->get_name()) != topologies.end()) {
                    ostringstream oss;
                    oss << "Topology '" << t->get_name()
                        << "' cannot be declared twice!";
                    throw runtime_error(oss.str());
                }

                // Goes over all edges and adds their source and end to
                // set<string> vertices.
                set<string> vertices;
                for (auto &edge : t->get_edges()) {
                    vertices.insert(edge->get_source()->get_name());
                    vertices.insert(edge->get_end()->get_name());
                }

                auto basic
                    = make_shared<topology_basic_t>(t->get_name(), vertices);
                for (auto &edge : t->get_edges()) {
                    basic->add_edge(
                        edge->get_source()->get_name(),
                        edge->get_end()->get_name()
                    );
                }

                topologies[t->get_name()] = basic;
            } else if (auto t
                       = dynamic_pointer_cast<ast_topology_linear_t>(decl)) {
                // Check for double definition
                if (topologies.find(t->get_name()) != topologies.end()) {
                    ostringstream oss;
                    oss << "Topology '" << t->get_name()
                        << "' cannot be declared twice!";
                    throw runtime_error(oss.str());
                }

                auto linear = make_shared<topology_linear_t>(t->get_name());
                for (auto &tag : t->get_tags()) {
                    linear->add_tag(tag->get_name());
                }
                topologies[t->get_name()] = linear;
            }
        }
    }
    return topologies;
}

static map<string, shared_ptr<topology_t>> &add_expr_topologies(
    const shared_ptr<ast_node_t> &ast,
    map<string, shared_ptr<topology_t>> &topologies
) {
    if (auto source = dynamic_pointer_cast<ast_source_t>(ast)) {
        for (auto &decl : source->get_decls()) {
            if (auto t = dynamic_pointer_cast<ast_topology_expr_t>(decl)) {
                // Check for double definition
                if (topologies.find(t->get_name()) != topologies.end()) {
                    ostringstream oss;
                    oss << "Topology '" << t->get_name()
                        << "' cannot be declared twice!";
                    throw runtime_error(oss.str());
                }

                auto topology = make_shared<topology_basic_t>(t->get_name());
                topology      = construct_expr_topology(
                    t->get_expr(), topologies, topology
                );

                topology->set_name_prefix(t->get_name());
                topologies[t->get_name()] = topology;
            }
        }
    }
    return topologies;
}

static shared_ptr<topology_basic_t> construct_expr_topology(
    shared_ptr<ast_expr_t> &expr,
    map<string, shared_ptr<topology_t>> &topologies,
    shared_ptr<topology_basic_t> &arg
) {
    if (auto e = dynamic_pointer_cast<ast_expr_bin_t>(expr)) {
        auto lhs = construct_expr_topology(e->get_lhs(), topologies, arg);
        auto rhs = construct_expr_topology(e->get_rhs(), topologies, arg);
        switch (e->get_oper()) {
            case ast_expr_bin_t::Oper::SUM:
                {
                    arg->disjoint_union(lhs, rhs);
                    return arg;
                }
            case ast_expr_bin_t::Oper::MUL:
                {
                    arg->carthesian_product(lhs, rhs);
                    return arg;
                }
            default:
                throw runtime_error("Unsupported binary operaion!");
        }
    } else if (auto e = dynamic_pointer_cast<ast_tag_t>(expr)) {
        try {
            shared_ptr<topology_t> &t = topologies.at(e->get_name());
            if (auto tl = dynamic_pointer_cast<topology_linear_t>(t)) {
                return make_shared<topology_basic_t>(*tl);
            } else if (auto tb = dynamic_pointer_cast<topology_basic_t>(t)) {
                return tb;
            }
            ostringstream oss;
            oss << "Topology '" << e->get_name()
                << "' should be linear or basic!";
            throw runtime_error(oss.str());
        } catch (out_of_range &err) {
            ostringstream oss;
            oss << "Unknown topology: '" << e->get_name() << "'!";
            throw runtime_error(oss.str());
        }
    } else {
        throw runtime_error("Unknown expression!");
    }
}

static map<string, shared_ptr<aware_t>> get_awares(
    const shared_ptr<ast_node_t> &ast, const topology_basic_t &topology
) {
    map<string, shared_ptr<aware_t>> topologies;

    return topologies;
}

static vector<pg_t> get_pgs(
    const shared_ptr<ast_node_t> &ast, const topology_basic_t &topology
) {
    vector<pg_t> perimeter_guards;
    if (auto source = dynamic_pointer_cast<ast_source_t>(ast)) {
        for (auto &decl : source->get_decls()) {
            if (auto t = dynamic_pointer_cast<ast_pg_t>(decl)) {
                try {
                    auto tag = topology.get_index(t->get_tag());
                    pg_t pg(t->get_name(), t->get_file(), tag);
                    perimeter_guards.push_back(pg);
                } catch (out_of_range &e) {
                    ostringstream oss;
                    oss << "Unknown tag for perimeter guard '" << t->get_name()
                        << "': '" << t->get_tag() << "'!";
                    throw runtime_error(oss.str());
                }
            }
        }
    }
    return perimeter_guards;
}

topology_basic_t::topology_basic_t(const string &n) {
    name      = n;
    mvertices = vector<vector<uint8_t>>();
    toindex   = map<string, int>();
    fromindex = map<int, string>();
}

topology_basic_t::topology_basic_t(
    const string &n, const set<string> &vertices
) {
    name      = n;
    mvertices = vector<vector<uint8_t>>(
        vertices.size(), vector<uint8_t>(vertices.size())
    );
    toindex   = map<string, int>();
    fromindex = map<int, string>();
    int i     = 0;
    for (auto &v : vertices) {
        toindex[fullname(v)] = i;
        fromindex[i]         = fullname(v);
        mvertices[i][i]      = 1;
        i++;
    }
}

topology_basic_t::topology_basic_t(topology_linear_t &t) {
    name      = t.get_name();
    size_t n  = t.get_tags().size();
    mvertices = vector<vector<uint8_t>>(n, vector<uint8_t>(n));
    for (size_t i = 0; i < n; i++) {
        mvertices[i][i] = 1;
        if (i + 1 < mvertices.size()) {
            mvertices[i][i + 1] = 1;
        }
        string tag   = remove_space(t.get_tags().at(i));
        toindex[tag] = i;
        fromindex[i] = tag;
    }
}

void topology_basic_t::add_edge(const string &source, const string &end) {
    int i           = toindex.at(fullname(source));
    int j           = toindex.at(fullname(end));
    mvertices[i][j] = 1;
}

void topology_basic_t::print() {
    cout << "Topology: '" << name << "'" << endl;
    for (auto &kv : toindex) {
        cout << "\t'" << kv.first << "', " << kv.second << ":";
        for (size_t j = 0; j < mvertices.size(); j++) {
            cout << " " << (int) mvertices[kv.second][j];
        }
        cout << endl;
    }
}

void topology_basic_t::carthesian_product(
    const shared_ptr<topology_basic_t> &t1,
    const shared_ptr<topology_basic_t> &t2
) {
    size_t n        = t1->size();
    size_t m        = t2->size();
    auto &a         = t1->matrix();
    auto &b         = t2->matrix();
    auto &mapping_a = t1->index_mapping();
    auto &mapping_b = t2->index_mapping();

    map<string, int> r_toindex;
    map<int, string> r_fromindex;
    for (auto &tuple_a : mapping_a) {
        for (auto &tuple_b : mapping_b) {
            string name    = "(" + tuple_a.first + "," + tuple_b.first + ")";
            int index      = tuple_a.second * m + tuple_b.second;
            string tag     = remove_space(name);
            r_toindex[tag] = index;
            r_fromindex[index] = tag;
        }
    }
    toindex.clear();
    toindex = r_toindex;
    fromindex.clear();
    fromindex = r_fromindex;

    vector<vector<uint8_t>> r(n * m, vector<uint8_t>(n * m));

    /* Do R = A (x) I_2 */
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            for (size_t ri = 0; ri < m; ri++) {
                r[i * m + ri][j * m + ri] = a[i][j];
            }
        }
    }

    /* Do R += I_1 (x) B */
    for (size_t ri = 0; ri < m * n; ri += m) {
        for (size_t i = 0; i < m; i++) {
            for (size_t j = 0; j < m; j++) {
                r[ri + i][ri + j] |= b[i][j];
            }
        }
    }

    for (size_t i = 0; i < mvertices.size(); i++) {
        mvertices[i].clear();
    }
    mvertices.clear();
    mvertices = r;
}

void topology_basic_t::disjoint_union(
    const shared_ptr<topology_basic_t> &t1,
    const shared_ptr<topology_basic_t> &t2
) {
    size_t n        = t1->size();
    size_t m        = t2->size();
    auto &a         = t1->matrix();
    auto &b         = t2->matrix();
    auto &mapping_a = t1->index_mapping();
    auto &mapping_b = t2->index_mapping();

    map<string, int> r_toindex;
    map<int, string> r_fromindex;
    for (auto &tuple : mapping_a) {
        string tag                = remove_space(tuple.first);
        r_toindex[tag]            = tuple.second;
        r_fromindex[tuple.second] = tag;
    }
    for (auto &tuple : mapping_b) {
        string tag                    = remove_space(tuple.first);
        r_toindex[tag]                = n + tuple.second;
        r_fromindex[n + tuple.second] = tag;
    }
    toindex.clear();
    toindex = r_toindex;
    fromindex.clear();
    fromindex = r_fromindex;

    vector<vector<uint8_t>> r(n + m, vector<uint8_t>(n + m));

    /* Perform direct sum of matrices */
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            r[i][j] = a[i][j];
        }
    }

    for (size_t i = 0; i < m; i++) {
        for (size_t j = 0; j < m; j++) {
            r[n + i][n + j] = b[i][j];
        }
    }

    for (size_t i = 0; i < mvertices.size(); i++) {
        mvertices[i].clear();
    }
    mvertices.clear();
    mvertices = r;
}

void topology_basic_t::set_name_prefix(const string &prefix) {
    map<string, int> updated_to;
    map<int, string> updated_from;
    for (auto &t : toindex) {
        string r               = remove_space(prefix + "." + t.first);
        updated_to[r]          = t.second;
        updated_from[t.second] = r;
    }
    toindex.clear();
    toindex = updated_to;
    fromindex.clear();
    fromindex = updated_from;
}

string topology_t::fullname(const string &tag) {
    return remove_space(name + "." + tag);
}

bool policy_t::contains_tag(const string &tag) const {
    string cleaned = remove_space(tag);
    return tags.find(cleaned) != tags.end();
}

static inline string remove_space(const string &s) {
    string r = s;
    r.erase(remove_if(r.begin(), r.end(), ::isspace), r.end());
    return r;
}

int policy_t::tag_index(const string &tag) const {
    return topology->get_index(tag);
}

int topology_basic_t::get_index(const string &tag) const {
    return toindex.at(remove_space(tag));
}

string topology_basic_t::get_tag(int index) const {
    return fromindex.at(index);
}

int topology_linear_t::get_index(const string &tag) const {
    string cleaned = remove_space(tag);
    for (size_t i = 0; i < tags.size(); i++) {
        if (tags[i] == cleaned) {
            return i;
        }
    }
    ostringstream oss;
    oss << "Tag '" << tag << "' not in the topology!";
    throw runtime_error(oss.str());
}

void topology_basic_t::add_unknown() {
    auto new_toindex   = map<string, int>();
    auto new_fromindex = map<int, string>();
    for (auto &t : toindex) {
        new_toindex[t.first]        = t.second + 1;
        new_fromindex[t.second + 1] = t.first;
    }
    new_toindex["unknown"] = 0;
    new_fromindex[0]       = "unknown";
    toindex.clear();
    toindex = new_toindex;
    fromindex.clear();
    fromindex = new_fromindex;

    for (auto &row : mvertices) {
        row.emplace(row.begin(), 0);
    }
    auto unknowns = vector<uint8_t>(mvertices.size() + 1, 1);
    mvertices.emplace(mvertices.begin(), unknowns);
}

void policy_t::dump(ofstream &out) {
    out << topology->size() << " " << perimeter_guards.size() << endl;
    for (size_t i = 0; i < lca_matrix.size(); i++) {
        out << topology->get_tag(i);
        for (size_t j = 0; j < lca_matrix[i].size(); j++) {
            out << " " << (int) lca_matrix[i][j];
        }
        out << endl;
    }

    for (auto &pg : perimeter_guards) {
        out << pg.name << " \"" << pg.file << "\" " << (int) pg.tag << endl;
    }
}

static void topological_sort_dfs(
    const vector<vector<uint8_t>> &m,
    const int index,
    vector<bool> &discovered,
    vector<int> &end_time,
    int &time,
    list<int> &topological_order
) {
    discovered[index] = true;
    for (size_t j = 0; j < m[index].size(); j++) {
        if (m[index][j] > 0 && !discovered[j]) {
            topological_sort_dfs(
                m, j, discovered, end_time, time, topological_order
            );
        }
    }
    end_time[index] = time;
    topological_order.push_front(index);
    time++;
}

static list<int> topological_ordering(const vector<vector<uint8_t>> &m) {
    vector<bool> discovered(m.size());
    vector<int> end_time(m.size());
    int time = 0;
    list<int> topological_order;

    for (size_t i = 0; i < m.size(); i++) {
        if (!discovered[i]) {
            topological_sort_dfs(
                m, i, discovered, end_time, time, topological_order
            );
        }
    }

    for (size_t i = 0; i < m.size(); i++) {
        for (size_t j = 0; j < m.size(); j++) {
            if (i != j && m[i][j] > 0 && end_time[i] <= end_time[j]) {
                ostringstream oss;
                oss << "The policy is not a directed acyclical graph!"; // <<
                throw runtime_error(oss.str());
            }
        }
    }
    return topological_order;
}

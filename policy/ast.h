#ifndef _POLICY_AST_H_
#define _POLICY_AST_H_

#include "policy.h"
#include "synan.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace std;

/* Interfaces */

/* Basic AST node */
class ast_node_t {
  public:
    virtual ~ast_node_t() = 0;
    virtual void print()  = 0;
};

/* Represents an expression - used for tags and expression topologies */
class ast_expr_t : public ast_node_t {
  public:
    virtual ~ast_expr_t() = 0;

    virtual void print() {}
};

/* Represents a declaration */
class ast_decl_t : public ast_node_t {
  public:
    virtual ~ast_decl_t() = 0;

    virtual void print() {}
};

/* Topology base class */
class ast_topology_t : public ast_decl_t {
  public:
    virtual ~ast_topology_t() {}

    void set_name(const string &n) {
        name = n;
    }

    string &get_name() {
        return name;
    }

    virtual void print() {}

  protected:
    string name;
};

/* Derived classes */
class ast_pg_t : public ast_decl_t {
  public:
    ast_pg_t(const string &file, const string &tag) : tag(tag), file(file) {}

    ~ast_pg_t() {}

    void set_name(const string &n) {
        name = n;
    }

    void print() {
        cout << name << ": " << file << " -> " << tag << endl;
    }

    string get_name() {
        return name;
    }

    string get_tag() {
        return tag;
    }

    string get_file() {
        return file;
    }

  private:
    string name;
    string tag;
    string file;
};

class ast_aware_t : public ast_decl_t {
  public:
    ast_aware_t(string n, shared_ptr<ast_topology_t> t) :
        name(n), topology(t) {}

    ~ast_aware_t() {}

    string &get_name() {
        return name;
    }

    shared_ptr<ast_topology_t> &get_topology() {
        return topology;
    }

    void print() {
        topology->print();
    }

  private:
    string name;
    shared_ptr<ast_topology_t> topology;
};

class ast_source_t : public ast_node_t {
  private:
    vector<shared_ptr<ast_decl_t>> decls;

  public:
    ast_source_t() {}

    void add_decl(const shared_ptr<ast_decl_t> &d) {
        decls.push_back(d);
    }

    vector<shared_ptr<ast_decl_t>> &get_decls() {
        return decls;
    }

    void print() {
        for (auto &d : decls) {
            d.get()->print();
        }
    }
};

class ast_decls_t : public ast_node_t {
  private:
    vector<shared_ptr<ast_decl_t>> decls;

  public:
    ast_decls_t() {};

    vector<shared_ptr<ast_decl_t>> &get_decls() {
        return decls;
    }

    void add(const shared_ptr<ast_decl_t> &d) {
        decls.push_back(d);
    }

    void print() {
        for (auto &d : decls) {
            d.get()->print();
        }
    }
};

class ast_tag_t : public ast_expr_t {
  private:
    string name;

  public:
    ast_tag_t();

    ast_tag_t(const string &n) : name(n) {}

    ~ast_tag_t() {}

    string &get_name() {
        return name;
    }

    void print() {
        cout << "\tTag '" << name << "'" << endl;
    }
};

class ast_edge_t : public ast_node_t {
  public:
    ast_edge_t(const string &s, const string &e) :
        source(new ast_tag_t(s)), end(new ast_tag_t(e)) {}

    void print() {
        source.get()->print();
        cout << "\t--->" << endl;
        end.get()->print();
    }

    shared_ptr<ast_tag_t> &get_source() {
        return source;
    }

    shared_ptr<ast_tag_t> &get_end() {
        return end;
    }

  private:
    shared_ptr<ast_tag_t> source;
    shared_ptr<ast_tag_t> end;
};

class ast_topology_basic_t : public ast_topology_t {
  public:
    void add_edge(const shared_ptr<ast_edge_t> &e) {
        edges.push_back(e);
    }

    vector<shared_ptr<ast_edge_t>> get_edges() {
        return edges;
    }

    void print() {
        cout << "Basic topology '" << name << "'" << endl;
        cout << "Edges: " << endl;
        for (auto &e : edges) {
            e.get()->print();
        }
    }

  private:
    vector<shared_ptr<ast_edge_t>> edges;
};

class ast_edges_t : public ast_node_t {
  public:
    void print() {
        for (auto &e : edges) {
            e.get()->print();
        }
    }

    vector<shared_ptr<ast_edge_t>> &get_edges() {
        return edges;
    }

    void add_edge(const shared_ptr<ast_edge_t> &e) {
        edges.push_back(e);
    }

  private:
    vector<shared_ptr<ast_edge_t>> edges;
};

class ast_topology_linear_t : public ast_topology_t {
  public:
    void add_tag(const shared_ptr<ast_tag_t> &t) {
        tags.push_back(t);
    }

    vector<shared_ptr<ast_tag_t>> &get_tags() {
        return tags;
    }

    void print() {
        cout << "Linear topology: " << endl;
        for (auto &t : tags) {
            t.get()->print();
        }
    }

  private:
    vector<shared_ptr<ast_tag_t>> tags;
};

class ast_topology_expr_t : public ast_topology_t {
  public:
    ast_topology_expr_t(const shared_ptr<ast_expr_t> &e) : expr(e) {}

    void print() {
        cout << "Expr topology: '" << name << "'" << endl;
        expr->print();
    }

    shared_ptr<ast_expr_t> &get_expr() {
        return expr;
    }

  private:
    shared_ptr<ast_expr_t> expr;
};

class ast_expr_bin_t : public ast_expr_t {
  public:
    enum class Oper {
        SUM,
        MUL
    };

    ast_expr_bin_t(
        const Oper o,
        const shared_ptr<ast_expr_t> &l,
        const shared_ptr<ast_expr_t> &r
    ) :
        oper(o), lhs(l), rhs(r) {}

    void set_lhs(const shared_ptr<ast_expr_t> &l) {
        lhs = l;
    }

    shared_ptr<ast_expr_t> &get_lhs() {
        return lhs;
    }

    void set_rhs(const shared_ptr<ast_expr_t> &r) {
        rhs = r;
    }

    shared_ptr<ast_expr_t> &get_rhs() {
        return rhs;
    }

    Oper get_oper() {
        return oper;
    }

    void print() {
        cout << "\tLeft side:" << endl << "\t";
        lhs.get()->print();
        cout << "\t\t" << (oper == Oper::MUL ? "*" : "+") << endl << "\t";
        cout << "\tRight side:" << endl << "\t";
        rhs.get()->print();
    }

  private:
    Oper oper;
    shared_ptr<ast_expr_t> lhs;
    shared_ptr<ast_expr_t> rhs;
};

shared_ptr<ast_node_t> ast_construct(
    const dertree_t &node, shared_ptr<ast_node_t> arg
);

#endif

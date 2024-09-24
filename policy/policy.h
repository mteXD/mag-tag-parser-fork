#ifndef _POLICY_POLICY_H_
#define _POLICY_POLICY_H_

#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace std;

class topology_t {
  public:
    virtual ~topology_t() {}

    string &get_name() {
        return name;
    }

    virtual void print() {
        cout << name << endl;
    }

    virtual string fullname(const string &tag);
    virtual int get_index(const string &tag) const = 0;

  protected:
    string name;
};

class topology_linear_t : public topology_t {
  public:
    topology_linear_t(const string &n) {
        name = n;
        tags = vector<string>();
    }

    topology_linear_t(const string &n, const vector<string> &ts) : tags(ts) {
        name = n;
    }

    void add_tag(const string &tag) {
        string fullname = name + "." + tag;
        tags.push_back(fullname);
    }

    vector<string> &get_tags() {
        return tags;
    }

    void print() {
        cout << "Topology '" << name << "'" << endl;
        cout << "\t";
        for (auto &t : tags) {
            cout << t << ",";
        }
        cout << endl;
    }

    int get_index(const string &tag) const;

  private:
    vector<string> tags;
};

class topology_basic_t : public topology_t {
  public:
    topology_basic_t(const string &n);
    topology_basic_t(const string &n, const set<string> &vertices);
    /**
     * @brief Converts linear topology to basic
     * @param t: reference to linear topology (topology_linear_t)
     */
    topology_basic_t(topology_linear_t &t);

    void add_edge(const string &source, const string &end);

    size_t size() const {
        return mvertices.size();
    }

    const vector<vector<uint8_t>> &matrix() const {
        return mvertices;
    }

    map<string, int> &index_mapping() {
        return toindex;
    }

    void disjoint_union(
        const shared_ptr<topology_basic_t> &t1,
        const shared_ptr<topology_basic_t> &t2
    );
    void carthesian_product(
        const shared_ptr<topology_basic_t> &t1,
        const shared_ptr<topology_basic_t> &t2
    );
    void print();
    void set_name_prefix(const string &prefix);
    int get_index(const string &tag) const;
    string get_tag(int index) const;
    void add_unknown();

  private:
    vector<vector<uint8_t>> mvertices;
    map<string, int> toindex;
    map<int, string> fromindex;
};

struct pg_t {
    string name;
    string file;
    uint8_t tag;

    pg_t(const string &n, const string &f, const uint8_t tag) :
        name(n), file(f), tag(tag) {}
};

struct aware_t {
    string name;
    shared_ptr<topology_t> topology;

    aware_t(const string &n, const shared_ptr<topology_t> t) :
        name(n), topology(t) {}
};

class policy_t {
  public:
    policy_t() {}

    policy_t(const char *file_path);
    bool contains_tag(const string &tag) const;
    int tag_index(const string &tag) const;

    void set_lca_matrix(const vector<vector<uint8_t>> lca) {
        lca_matrix = lca;
    }

    const vector<vector<uint8_t>> &get_lca_matrix() const {
        return lca_matrix;
    }

    void dump(ofstream &out);

    /**
     * @brief mash of all topologies (at some point, they were all converted to
     * topology_basic_t, which explains the pointer type)
     */
    shared_ptr<topology_basic_t> topology;

  private:
    /**
     * @brief resolves {topoloy name}->{pointer to topology}, holds topologies
     * as described in the policy file.
     */
    map<string, shared_ptr<topology_t>> topologies;
    /**
     * @brief holds names of all the tags. Eases `shared_ptr<topology_basic_t>
     * topology` usage
     */
    set<string> tags;
    vector<vector<uint8_t>> lca_matrix;
    vector<pg_t> perimeter_guards;

    map<string, shared_ptr<aware_t>> aware_connections;
};

#endif

#include "synan.h"

#include "lexer.h"

// #include <exception>
#include <sstream>

static dertree_t parse_decls(std::vector<symbol_t> &symbols);
static dertree_t parse_decl(std::vector<symbol_t> &symbols);
static dertree_t parse_declrest(std::vector<symbol_t> &symbols);
static dertree_t parse_topology(std::vector<symbol_t> &symbols);
static dertree_t parse_aware(std::vector<symbol_t> &symbols);
static dertree_t parse_topology_rest(std::vector<symbol_t> &symbols);
static dertree_t parse_basic(std::vector<symbol_t> &symbols);
static dertree_t parse_edge(std::vector<symbol_t> &symbols);
static dertree_t parse_edge_rest(std::vector<symbol_t> &symbols);
static dertree_t parse_linear(std::vector<symbol_t> &symbols);
static dertree_t parse_linear_rest(std::vector<symbol_t> &symbols);
static dertree_t parse_expr(std::vector<symbol_t> &symbols);
static dertree_t parse_sum(std::vector<symbol_t> &symbols);
static dertree_t parse_sum_rest(std::vector<symbol_t> &symbols);
static dertree_t parse_mul(std::vector<symbol_t> &symbols);
static dertree_t parse_mul_rest(std::vector<symbol_t> &symbols);
static dertree_t parse_elem(std::vector<symbol_t> &symbols);
static dertree_t parse_pg(std::vector<symbol_t> &symbols);
static dertree_t parse_pg_rest(std::vector<symbol_t> &symbols);

//

static inline std::string error_msg(
    const symbol_t &s, const std::string expected
);
static inline symbol_t &peek(
    std::vector<symbol_t> &symbols, const std::string err
);
static inline symbol_t &consume(
    std::vector<symbol_t> &symbols, const std::string err
);
static void add_leaf(
    dertree_t &t,
    std::vector<symbol_t> &symbols,
    Term expected_symbol,
    const std::string &err
);
/******************************************************************************/

size_t symbol_index = 0;

// Helper inline functions.

/**
 * Helper function for exception throwing.
 *
 * @param s: symbol_t token (contains line and column number)
 * @param expected: error message
 */
static inline std::string error_msg(
    const symbol_t &s, const std::string expected
) {
    std::ostringstream oss;
    oss << "Expected " << expected << ", got '" << s.name
        << "'! Location: " << s.line << ", " << s.column;
    return oss.str();
}

/**
 * Helper function for obtaining the next symbol_t token. Throws an exception
 * if it doesn't exist.
 * at() function takes from the symbol_index  position or throws an error if
 * it's out of range
 *
 * @param symbols: token vector
 * @param err: error message
 */
static inline symbol_t &peek(
    std::vector<symbol_t> &symbols, const std::string err
) {
    try {
        return symbols.at(symbol_index);
    } catch (std::out_of_range &e) {
        throw std::runtime_error(err);
    }
}

/**
 * Helper function for moving along the vector (it increases the symbol_index).
 * Throws an exception if out of range.
 *
 * @param symbols: symbol_t token vector.
 * @param err: error message
 */
static inline symbol_t &consume(
    std::vector<symbol_t> &symbols, const std::string err
) {
    try {
        symbol_index++;
        return symbols.at(symbol_index - 1); // TODO: unnecesarry `-1`?
    } catch (std::out_of_range &e) {
        throw std::runtime_error(err);
    }
}

/**
 * Helper function for adding terminals (leaves of the tree).
 *
 * Throws an error if the current token's type and expected_symbol (expeceted
 * type) don't match.
 *
 * @param t: dertree_t tree.
 * @param symbols: symbol_t token vector.
 * @param expected_symbol: expected type of terminal symbol.
 * @param err: error message (describes what's missing).
 */
static void add_leaf(
    dertree_t &t,
    std::vector<symbol_t> &symbols,
    Term expected_symbol,
    const std::string &err
) {
    auto &s = consume(symbols, "Missing " + err);
    if (s.term != expected_symbol) {
        throw std::runtime_error(error_msg(s, err));
    }
    t.leaves.push_back(s);
}

/**
 * @param symbols Takes a list of symbols (tokens, see `policy_t` constructor)
 * parses them and returns dertree_t, which is symbol analysis (not AST Abstract
 * Syntax Tree).
 *
 * This fucntion is mainly responsible for checking if the policy file is empty.
 * It is the first function in the chain of similar functions (`parse_*()`) that
 * are responsible for parsing the token list. To understand the parsing process
 * easier, refer to ./docs/cfg.txt.
 */
dertree_t parse_source(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::SOURCE;
    peek(symbols, "Policy file is empty!");
    t.subtrees.push_back(parse_decls(symbols));
    return t;
}

static dertree_t parse_decls(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::DECLS;
    t.subtrees.push_back(parse_decl(symbols));
    t.subtrees.push_back(parse_declrest(symbols));
    return t;
}

static dertree_t parse_decl(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::DECL;
    auto &s = peek(symbols, "Missing declarations!");
    switch (s.term) {
        case Term::TOPOLOGY:
            t.subtrees.push_back(parse_topology(symbols));
            break;
        case Term::PG: t.subtrees.push_back(parse_pg(symbols)); break;
        case Term::AWARE: t.subtrees.push_back(parse_aware(symbols)); break;
        default: throw std::runtime_error(error_msg(s, "declarations"));
    }
    return t;
}

static dertree_t parse_declrest(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::DECLREST;
    auto &s = peek(symbols, "Missing declarations!");
    switch (s.term) {
        case Term::END: break;
        case Term::TOPOLOGY:
            t.subtrees.push_back(parse_decl(symbols));
            t.subtrees.push_back(parse_declrest(symbols));
            break;
        default: throw std::runtime_error(error_msg(s, "declarations"));
    }
    return t;
}

static dertree_t parse_topology(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::TOPOLOGY;

    add_leaf(t, symbols, Term::TOPOLOGY, "'topology'");
    add_leaf(t, symbols, Term::IDENTIFIER, "an identifier");
    add_leaf(t, symbols, Term::COLON, "':'");
    t.subtrees.push_back(parse_topology_rest(symbols));

    return t;
}

static dertree_t parse_aware(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::AWARE;

    add_leaf(t, symbols, Term::AWARE, "'aware'");
    add_leaf(t, symbols, Term::IDENTIFIER, "an identifier");
    add_leaf(t, symbols, Term::COLON, "':'");
    // TODO: change names of parse_topology and parse_topology_rest to something
    // that better unifies Nont::TOPOLOGY and Nont::AWARE
    t.subtrees.push_back(parse_topology_rest(symbols));

    return t;
}

static dertree_t parse_topology_rest(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::TOPOLOGYREST;

    auto &s = consume(symbols, "Missing topology type!");
    t.leaves.push_back(s);

    switch (s.term) {
        case Term::BASIC:
            add_leaf(t, symbols, Term::LBRACE, "'{'");
            t.subtrees.push_back(parse_basic(symbols));
            add_leaf(t, symbols, Term::RBRACE, "'}");
            break;
        case Term::LINEAR: t.subtrees.push_back(parse_linear(symbols)); break;
        case Term::EXPR: t.subtrees.push_back(parse_expr(symbols)); break;
        default:
            std::ostringstream oss;
            oss << "Unsupported topology type '" << s.name
                << "'! Location: " << s.line << ", " << s.column;
            throw std::runtime_error(oss.str());
    }

    return t;
}

static dertree_t parse_basic(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::BASIC;
    t.subtrees.push_back(parse_edge(symbols));
    t.subtrees.push_back(parse_edge_rest(symbols));
    return t;
}

static dertree_t parse_edge(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::EDGE;

    add_leaf(t, symbols, Term::STRING, "a tag string");
    add_leaf(t, symbols, Term::ARROW, "'->'");
    add_leaf(t, symbols, Term::STRING, "a tag string");

    return t;
}

static dertree_t parse_edge_rest(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::EDGEREST;

    auto &s = peek(symbols, "Missing a ',' or '}'!");
    switch (s.term) {
        case Term::RBRACE: break;
        case Term::COMMA:
            t.leaves.push_back(s);
            consume(symbols, "Missing a ','!");
            t.subtrees.push_back(parse_edge(symbols));
            t.subtrees.push_back(parse_edge_rest(symbols));
            break;
        default: throw std::runtime_error(error_msg(s, "',' or '}'"));
    }
    return t;
}

static dertree_t parse_linear(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::LINEAR;

    add_leaf(t, symbols, Term::STRING, "a tag string");
    t.subtrees.push_back(parse_linear_rest(symbols));
    return t;
}

static dertree_t parse_linear_rest(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::LINEARREST;
    auto &s = peek(symbols, "Missing a ',' or declarations!");
    switch (s.term) {
        case Term::TOPOLOGY:
        case Term::END:
        case Term::PG: break;
        case Term::COMMA:
            t.leaves.push_back(s);
            consume(symbols, "Missing a ','!");
            t.subtrees.push_back(parse_linear(symbols));
            break;
        default: throw std::runtime_error(error_msg(s, ","));
    }

    return t;
}

static dertree_t parse_expr(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::EXPR;
    auto &s = peek(symbols, "Missing an identifier or expression!");
    switch (s.term) {
        case Term::IDENTIFIER:
        case Term::LPAREN: t.subtrees.push_back(parse_sum(symbols)); break;
        default: throw std::runtime_error(error_msg(s, "an identifier or '('"));
    }

    return t;
}

static dertree_t parse_sum(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::SUM;
    auto &s = peek(symbols, "Missing identifier or expression!");
    switch (s.term) {
        case Term::IDENTIFIER:
        case Term::LPAREN:
            t.subtrees.push_back(parse_mul(symbols));
            t.subtrees.push_back(parse_sum_rest(symbols));
            break;
        default: throw std::runtime_error(error_msg(s, "an identifier or '('"));
    }

    return t;
}

static dertree_t parse_sum_rest(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::SUMREST;
    auto &s = peek(symbols, "Missing end of expression or '+'!");
    switch (s.term) {
        case Term::TOPOLOGY:
        case Term::RPAREN:
        case Term::END:
        case Term::PG: break;
        case Term::PLUS:
            t.leaves.push_back(s);
            consume(symbols, "Missing a '+'!");
            t.subtrees.push_back(parse_mul(symbols));
            t.subtrees.push_back(parse_sum_rest(symbols));
            break;
        default:
            throw std::runtime_error(error_msg(s, " end of expression or '+'"));
    }

    return t;
}

static dertree_t parse_mul(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::MUL;
    auto &s = peek(symbols, "Missing identifier or expression!");
    switch (s.term) {
        case Term::IDENTIFIER:
        case Term::LPAREN:
            t.subtrees.push_back(parse_elem(symbols));
            t.subtrees.push_back(parse_mul_rest(symbols));
            break;
        default: throw std::runtime_error(error_msg(s, "an identifier or '('"));
    }
    return t;
}

static dertree_t parse_mul_rest(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::MULREST;
    auto &s = peek(symbols, "Missing end of expression!");
    switch (s.term) {
        case Term::TOPOLOGY:
        case Term::RPAREN:
        case Term::END:
        case Term::PLUS:
        case Term::PG: break;
        case Term::MULT:
            t.leaves.push_back(s);
            consume(symbols, "Missing a '*'");
            t.subtrees.push_back(parse_elem(symbols));
            t.subtrees.push_back(parse_mul_rest(symbols));
            break;
        default:
            throw std::runtime_error(error_msg(s, " end of expression or '*'"));
    }
    return t;
}

static dertree_t parse_elem(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::ELEM;
    auto &s = consume(symbols, "Missing an identifier or a nested expression!");
    switch (s.term) {
        case Term::IDENTIFIER: t.leaves.push_back(s); break;
        case Term::LPAREN:
            t.leaves.push_back(s);
            t.subtrees.push_back(parse_sum(symbols));
            add_leaf(t, symbols, Term::RPAREN, "')'");
            break;
        default:
            throw std::runtime_error(
                error_msg(s, "an identifier or a nested expression")
            );
    }
    return t;
}

static dertree_t parse_pg(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::PG;

    add_leaf(t, symbols, Term::PG, "'pg'");
    add_leaf(t, symbols, Term::IDENTIFIER, "an identifier");
    add_leaf(t, symbols, Term::LBRACE, "'{'");
    t.subtrees.push_back(parse_pg_rest(symbols));
    add_leaf(t, symbols, Term::RBRACE, "'}'");

    return t;
}

static dertree_t parse_pg_rest(std::vector<symbol_t> &symbols) {
    dertree_t t;
    t.label = Nont::PG_REST;

    add_leaf(t, symbols, Term::PG_FILE, "keyword 'file'");
    add_leaf(t, symbols, Term::COLON, "':'");
    add_leaf(
        t, symbols, Term::STRING,
        "a string containig \"filename\" or [\"stdin\"|\"stdout\"|\"stderr\"]"
    );
    add_leaf(t, symbols, Term::IDENTIFIER, "'tag'");
    if (t.leaves.back().name != "tag") {
        throw std::runtime_error(error_msg(t.leaves.back(), "'tag'"));
    }
    add_leaf(t, symbols, Term::EQUAL, "'='");
    add_leaf(t, symbols, Term::STRING, "a string");

    return t;
}

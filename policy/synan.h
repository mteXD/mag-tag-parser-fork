#ifndef _POLICY_SYNAN_H_
#define _POLICY_SYNAN_H_

#include "lexer.h"

#include <vector>

// Enum that defines non-terminals
enum class Nont {
    SOURCE,
    DECLS,
    DECLREST,
    DECL,
    TOPOLOGY,
    AWARE,
    TOPOLOGYREST,
    BASIC,
    EDGE,
    EDGEREST,
    LINEAR,
    LINEARREST,
    EXPR,
    SUM,
    SUMREST,
    MUL,
    MULREST,
    ELEM,
    PG,
    PG_REST
};

/**
 * Structure that holds the symbol analysis.
 * @param label: a non-terminal that corresponds with the current position along
 * of our symbol analysis.
 * @param subtrees: they hold non-terminals.
 * @param leaves: they hold terminals.
 */
struct dertree_t {
    Nont label;
    std::vector<dertree_t> subtrees;
    std::vector<symbol_t> leaves;
};

dertree_t parse_source(std::vector<symbol_t> &symbols);

#endif

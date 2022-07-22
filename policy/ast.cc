#include "ast.h"

#include <stdexcept>

ast_node_t::~ast_node_t() {}
ast_expr_t::~ast_expr_t() {}
ast_decl_t::~ast_decl_t() {}


std::shared_ptr<ast_node_t> ast_construct(const dertree_t& node, std::shared_ptr<ast_node_t> arg) {
	switch (node.label) {
		case Nont::SOURCE: {
			std::shared_ptr<ast_source_t> ps(new ast_source_t);
			auto pdecls = std::dynamic_pointer_cast<ast_decls_t>(ast_construct(node.subtrees.at(0), nullptr));
			for (auto& d : pdecls->get_decls()) {
				ps->add_decl(d);
			}
			return ps;
		}
		case Nont::DECLS:
		case Nont::DECLREST: {
			std::shared_ptr<ast_decls_t> pdecls(new ast_decls_t);
			if (node.subtrees.size() == 0) {
				return pdecls;
			}
			auto pdecl = std::dynamic_pointer_cast<ast_decl_t>(ast_construct(node.subtrees.at(0), std::shared_ptr<ast_node_t>()));
			pdecls->add(pdecl);
			auto prest = std::dynamic_pointer_cast<ast_decls_t>(ast_construct(node.subtrees.at(1), nullptr));
			for (auto& d : prest->get_decls()) {
				pdecls->add(d);
			}
			return pdecls;
		}
		case Nont::DECL:
			return ast_construct(node.subtrees.at(0), nullptr);
		case Nont::TOPOLOGY: {
			auto pt = std::dynamic_pointer_cast<ast_topology_t>(ast_construct(node.subtrees.at(0), nullptr));
			pt->set_name(node.leaves.at(1).name);
			return pt;
		}
		case Nont::TOPOLOGYREST:
			return ast_construct(node.subtrees.at(0), nullptr);
		case Nont::BASIC: {
			std::shared_ptr<ast_topology_basic_t> ptb(new ast_topology_basic_t);
			auto e = std::dynamic_pointer_cast<ast_edge_t>(ast_construct(node.subtrees.at(0), nullptr));
			ptb->add_edge(e);
			auto es = std::dynamic_pointer_cast<ast_edges_t>(ast_construct(node.subtrees.at(1), nullptr));
			for (auto& e : es->get_edges()) {
				ptb->add_edge(e);
			}
			return ptb;
		}
		case Nont::EDGE:
			return std::shared_ptr<ast_edge_t>(new ast_edge_t(node.leaves.at(0).name, node.leaves.at(2).name));
		case Nont::EDGEREST: {
			std::shared_ptr<ast_edges_t> pes(new ast_edges_t);
			if (node.subtrees.size() == 0) {
				return pes;
			}
			auto e = std::dynamic_pointer_cast<ast_edge_t>(ast_construct(node.subtrees.at(0), nullptr));
			pes->add_edge(e);
			auto es = std::dynamic_pointer_cast<ast_edges_t>(ast_construct(node.subtrees.at(1), nullptr));
			for (auto& e : es->get_edges()) {
				pes->add_edge(e);
			}
			return pes;
		}
		case Nont::LINEAR: {
			std::shared_ptr<ast_topology_linear_t> ptl(new ast_topology_linear_t);
			std::shared_ptr<ast_tag_t> pt(new ast_tag_t(node.leaves.at(0).name));
			ptl->add_tag(pt);
			auto prest = std::dynamic_pointer_cast<ast_topology_linear_t>(ast_construct(node.subtrees.at(0), nullptr));
			for (auto& e : prest->get_tags()) {
				ptl->add_tag(e);
			}
			return ptl;
		}
		case Nont::LINEARREST: {
			return (node.subtrees.size() == 0) ?
				std::shared_ptr<ast_topology_linear_t>(new ast_topology_linear_t) :
				ast_construct(node.subtrees.at(0), nullptr);
		}
		case Nont::EXPR: {
			auto pe = std::dynamic_pointer_cast<ast_expr_t>(ast_construct(node.subtrees.at(0), nullptr));
			return std::shared_ptr<ast_topology_expr_t>(new ast_topology_expr_t(pe));
		}
		case Nont::SUM: case Nont::MUL: {
			auto plhs = ast_construct(node.subtrees.at(0), nullptr);
			return ast_construct(node.subtrees.at(1), plhs);
		}
		case Nont::MULREST: case Nont::SUMREST: {
			if (node.subtrees.size() == 0) {
				return arg;
			}
			auto prhs = std::dynamic_pointer_cast<ast_expr_t>(ast_construct(node.subtrees.at(0), nullptr));
			std::shared_ptr<ast_expr_bin_t> expr(new ast_expr_bin_t(
				((node.label == Nont::SUMREST) ? ast_expr_bin_t::Oper::SUM : ast_expr_bin_t::Oper::MUL),
				std::dynamic_pointer_cast<ast_expr_t>(arg), prhs));
			return ast_construct(node.subtrees.at(1), expr);
		}
		case Nont::ELEM: {
			return (node.subtrees.size() == 0) ?
				std::shared_ptr<ast_tag_t>(new ast_tag_t(node.leaves.at(0).name)) :
				ast_construct(node.subtrees.at(0), nullptr);
		}
		default:
			throw std::runtime_error("Unknown syntax!");
	}
}
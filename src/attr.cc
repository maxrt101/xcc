#include "xcc/attrs.h"
#include "xcc/util/log.h"
#include "xcc/xcc.h"
#include <unordered_map>

using namespace xcc;

static auto& logger = xcc::log::Logger::get("ATTR");

static void xcc_attr_exclude(codegen::GlobalContext& globalContext, const ast::Node::Attribute& attr, ast::Node * node) {
  // Soundness: it's okay to slice objects here, because Empty has no additional
  //            fields in relation to Node, so it shouldn't actually get sliced
  *node = ast::Empty();
}

static void xcc_attr_dump_ast(codegen::GlobalContext& globalContext, const ast::Node::Attribute& attr, ast::Node * node) {
  logger.print("{}\n", node->toString(nullptr, nullptr, 0, true));
}

static void xcc_attr_if(codegen::GlobalContext& globalContext, const ast::Node::Attribute& attr, ast::Node * node) {
  assertRaiseFromNode(attr.args.size() == 1, Error(ERROR_ATTR_ARG_COUNT_MISMATCH, attr.span), node);

  assertRaiseFromNode(attr.args[0]->is(ast::AST_EXPR_NUMBER),
    Error(ERROR_ATTR_ARG_TYPE_MISMATCH, attr.span, "Argument to [if] must evaluate to a number"), node);

  auto result = attr.args[0]->as<ast::Number>();

  assertRaiseFromNode(result->tag == ast::Number::INTEGER,
    Error(ERROR_ATTR_ARG_TYPE_MISMATCH, attr.span, "Argument to [if] must evaluate to an integer"), node);

  if (!result->value.integer) {
    // Soundness: it's okay to slice objects here, because Empty has no additional
    //            fields in relation to Node, so it shouldn't actually get sliced
    *node = ast::Empty();
  }
}

static std::unordered_map<std::string, attr::Handler> attr_handlers = {
  // Built-ins
  {"exclude",    xcc_attr_exclude},
  {"if",         xcc_attr_if},

  // Debug
  {"__dump_ast", xcc_attr_dump_ast},

  // __dump_fn_ir
  // __dump_module_ir
  // if(const/macros)
};

void attr::registerHandler(const std::string& name, Handler handler) {
  attr_handlers[name] = std::move(handler);
}

void attr::callHandler(codegen::GlobalContext& globalContext, const ast::Node::Attribute& attr, ast::Node * node) {
  if (attr_handlers.contains(attr.name)) {
    attr_handlers[attr.name](globalContext, attr, node);
  }
}

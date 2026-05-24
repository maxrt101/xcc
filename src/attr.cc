#include "xcc/attrs.h"
#include "xcc/util/log.h"
#include <unordered_map>

using namespace xcc;

static auto& logger = xcc::log::Logger::get("ATTR");

static void xcc_attr_exclude(const ast::Node::Attribute& attr, ast::Node* node) {
  // Soundness: it's okay to slice objects here, because Empty has no additional
  //            fields in relation to Node, so it shouldn't get sliced
  *node = ast::Empty();
}

static void xcc_attr_dump_ast(const ast::Node::Attribute& attr, ast::Node* node) {
  logger.print("{}\n", node->toString(nullptr, nullptr, 0, true));
}

static std::unordered_map<std::string, attr::Handler> attr_handlers = {
  // Built-ins
  {"exclude", xcc_attr_exclude},

  // Debug
  {"__dump_ast", xcc_attr_dump_ast}

  // __dump_fn_ir
  // __dump_module_ir
  // if(const/macros)
};

void attr::registerHandler(const std::string& name, Handler handler) {
  attr_handlers[name] = std::move(handler);
}

void attr::callHandler(const ast::Node::Attribute& attr, ast::Node * node) {
  if (attr_handlers.contains(attr.name)) {
    attr_handlers[attr.name](attr, node);
  }
}

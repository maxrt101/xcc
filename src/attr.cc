#include "xcc/attrs.h"
#include <unordered_map>

using namespace xcc;

static void xcc_attr_exclude(const ast::Node::Attribute& attr, ast::Node* node) {
  // Soundness: it's okay to slice objects here, because Empty has no additional
  //            fields in relation to Node, so it shouldn't get sliced
  *node = ast::Empty();
}

static void xcc_attr_dump_ast(const ast::Node::Attribute& attr, ast::Node* node) {
  ast::printNode(node, node, 0);
}

static std::unordered_map<std::string, attr::Handler> attr_handlers = {
  // Built-ins
  {"exclude", xcc_attr_exclude},

  // Debug
  {"__dump_ast", xcc_attr_dump_ast}
};

void attr::registerHandler(const std::string& name, Handler handler) {
  attr_handlers[name] = handler;
}

void attr::callHandler(const ast::Node::Attribute& attr, ast::Node * node) {
  if (attr_handlers.contains(attr.name)) {
    attr_handlers[attr.name](attr, node);
  }
}

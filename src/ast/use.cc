#include "xcc/ast/use.h"

using namespace xcc::ast;

Use::Use(std::shared_ptr<Node> name) : Node(AST_USE), name(std::move(name)) {}

std::shared_ptr<Use> Use::create(std::shared_ptr<Node> name) {
  return std::make_shared<Use>(std::move(name));
}


#include "xcc/ast/use.h"

using namespace xcc::ast;

Use::Use(std::shared_ptr<Node> name, std::shared_ptr<Block> items) : Node(AST_USE), name(std::move(name)), items(std::move(items)) {}

std::shared_ptr<Use> Use::create(std::shared_ptr<Node> name, std::shared_ptr<Block> items) {
  return std::make_shared<Use>(std::move(name), std::move(items));
}


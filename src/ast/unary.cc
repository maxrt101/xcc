#include "xcc/ast/unary.h"

using namespace xcc::ast;

Unary::Unary(Token operation, std::shared_ptr<Node> rhs)
    : Node(AST_EXPR_UNARY), operation(operation), rhs(rhs) {}

std::shared_ptr<Unary> Unary::create(Token operation, std::shared_ptr<Node> rhs) {
  return std::make_shared<Unary>(std::move(operation), std::move(rhs));
}

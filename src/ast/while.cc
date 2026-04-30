#include "xcc/ast/while.h"
#include "xcc/exceptions.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

While::While(std::shared_ptr<Node> condition, std::shared_ptr<Node> body)
  : Node(AST_WHILE), condition(std::move(condition)), body(std::move(body)) {}

std::shared_ptr<While> While::create(std::shared_ptr<Node> condition, std::shared_ptr<Node> body) {
  return std::make_shared<While>(std::move(condition), std::move(body));
}

std::shared_ptr<Node> While::clone() {
  return withAttrs(create(condition->clone(), body->clone()));
}

void While::visit(Visitor visitor) {
  callVisitor(condition, visitor);
  callVisitor(body, visitor);
}

std::string While::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return std::format("while ({}) {}",
    condition->toString(parent, this, indent, false),
    body->toString(parent, this, indent, newline)
  );
}

llvm::Value * While::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  throw CodegenException("while loops are unsupported");
}

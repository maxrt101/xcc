#include "xcc/ast/while.h"
#include "xcc/exceptions.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

While::While(SourceSpan span, std::shared_ptr<Node> condition, std::shared_ptr<Node> body)
  : Node(AST_WHILE, span), condition(std::move(condition)), body(std::move(body)) {}

std::shared_ptr<While> While::create(SourceSpan span, std::shared_ptr<Node> condition, std::shared_ptr<Node> body) {
  return std::make_shared<While>(span, std::move(condition), std::move(body));
}

std::shared_ptr<Node> While::clone() {
  return withAttrs(create(span, condition->clone(), body->clone()));
}

void While::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(condition, visitor, ignoreSubtree);
  callVisitor(body, visitor, ignoreSubtree);
}

std::string While::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(indent, newline) + std::format("while ({}) {}",
    condition->toString(parent, this, indent, false),
    body->toString(parent, this, indent, newline)
  );
}

llvm::Value * While::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  Error(ERROR_UNIMPLEMENTED, span, "while loops are unsupported").raiseFromNode(this);
}

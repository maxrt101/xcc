#include "xcc/ast/cast.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

Cast::Cast(SourceSpan span, std::shared_ptr<Node> expr, std::shared_ptr<Node> type)
  : Node(AST_EXPR_CAST, span), expr(std::move(expr)), type(std::move(type)) {}

std::shared_ptr<Cast> Cast::create(SourceSpan span, std::shared_ptr<Node> expr, std::shared_ptr<Node> type) {
  return std::make_shared<Cast>(span, std::move(expr), std::move(type));
}

std::shared_ptr<Node> Cast::clone() {
  return withAttrs(create(span, expr->clone(), cast<Type>(type->clone())));
}

void Cast::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(expr, visitor, ignoreSubtree);
  callVisitor(type, visitor, ignoreSubtree);
}

std::string Cast::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(0, false) + std::format("{} as {}",
    expr->toString(parent, this, indent, false),
    type->toString(parent, this, indent, false)
  );
}

llvm::Value * Cast::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  return codegen::castIfNotSame(ctx, expr->generateValue(ctx, {}), type->generateType(ctx, {})->getLLVMType(ctx));
}

std::shared_ptr<xcc::meta::Type> Cast::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  return type->generateType(ctx, {});
}

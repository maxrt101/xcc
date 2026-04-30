#include "xcc/ast/cast.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

Cast::Cast(std::shared_ptr<Node> expr, std::shared_ptr<Node> type)
  : Node(AST_EXPR_CAST), expr(std::move(expr)), type(std::move(type)) {}

std::shared_ptr<Cast> Cast::create(std::shared_ptr<Node> expr, std::shared_ptr<Node> type) {
  return std::make_shared<Cast>(std::move(expr), std::move(type));
}

std::shared_ptr<Node> Cast::clone() {
  return withAttrs(create(expr->clone(), cast<Type>(type->clone())));
}

void Cast::visit(Visitor visitor) {
  callVisitor(expr, visitor);
  callVisitor(type, visitor);
}

std::string Cast::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return std::format("{} as {}",
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

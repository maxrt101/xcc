#include "xcc/ast/cast.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

Cast::Cast(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> expr, std::shared_ptr<Node> type)
  : Node(AST_EXPR_CAST, span, scope), expr(std::move(expr)), type(std::move(type)) {}

std::shared_ptr<Cast> Cast::create(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> expr, std::shared_ptr<Node> type) {
  return std::make_shared<Cast>(span, scope, std::move(expr), std::move(type));
}

std::shared_ptr<Node> Cast::clone() {
  return withAttrs(create(span, scope, expr->clone(), cast<Type>(type->clone())));
}

void Cast::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(globalContext, expr, visitor, ignoreSubtree);
  callVisitor(globalContext, type, visitor, ignoreSubtree);
}

std::string Cast::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(0, false) + std::format("{} as {}",
    expr->toString(parent, this, indent, false),
    type->toString(parent, this, indent, false)
  );
}

llvm::Value * Cast::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  return castIfNotSame(ctx, expr->generateValue(ctx, payload), type->generateType(ctx, payload)->getLLVMType(ctx), span);
}

std::shared_ptr<xcc::meta::Type> Cast::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  return type->generateType(ctx, payload);
}

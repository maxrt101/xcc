#include "xcc/ast/return.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

Return::Return(SourceSpan span, std::shared_ptr<Node> value)
  : Node(AST_RETURN, span), value(std::move(value)) {}

std::shared_ptr<Return> Return::create(SourceSpan span, std::shared_ptr<Node> value) {
  return std::make_shared<Return>(span, std::move(value));
}

std::shared_ptr<Node> Return::clone() {
  return withAttrs(create(span, value->clone()));
}

void Return::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(value, visitor, ignoreSubtree);
}

std::string Return::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(indent, newline) + std::format("return {}", value->toString(parent, this, indent, false));
}

llvm::Value * Return::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  llvm::Value * val = nullptr;

  if (value) {
    val = value->generateValue(ctx, {});

    if (auto fn = ctx.globalContext.getCurrentFunction()) {
      val = codegen::castIfNotSame(ctx, val, fn->getLLVMReturnType(ctx), value->span);
    }

    ctx.clearScopes();
    ctx.ir_builder->CreateRet(val);
  } else {
    ctx.clearScopes();
    ctx.ir_builder->CreateRetVoid();
  }

  return val;
}

std::shared_ptr<xcc::meta::Type> Return::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  if (value) {
    return value->generateType(ctx, {});
  }

  return xcc::meta::Type::createVoid();
}

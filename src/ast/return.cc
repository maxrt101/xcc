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

void Return::visit(std::unique_ptr<codegen::GlobalContext>& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(globalContext, value, visitor, ignoreSubtree);
}

std::string Return::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(indent, newline) + std::format("return {}", value->toString(parent, this, indent, false));
}

llvm::Value * Return::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  llvm::Value * val = nullptr;

  if (value) {
    std::shared_ptr<meta::Type> type;

    if (auto fn = ctx.globalContext.getCurrentFunction()) {
      type = fn->returnType;
    }

    val = value->generateValue(ctx, extendPayload(excludePayload(payload, AST_INIT), Initializer::Payload::create(type)));

    if (type) {
      val = castIfNotSame(ctx, val, type->getLLVMType(ctx), value->span);
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
    return value->generateType(ctx, payload);
  }

  return meta::Type::createVoid();
}

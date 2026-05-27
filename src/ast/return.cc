#include "xcc/ast/return.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

Return::Return(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> value)
  : Node(AST_RETURN, span, scope), value(std::move(value)) {}

std::shared_ptr<Return> Return::create(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> value) {
  return std::make_shared<Return>(span, scope, std::move(value));
}

std::shared_ptr<Node> Return::clone() {
  return withAttrs(create(span, scope, value ? value->clone() : nullptr));
}

void Return::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(globalContext, value, visitor, ignoreSubtree);
}

std::string Return::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(indent, newline) + std::format("return{}", value ? (" " + value->toString(parent, this, indent, false)) : "");
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

    // If last value is an identifier - forget it, so it doesn't get destroyed,
    // as it needs to live long enough to be returned from the function
    if (auto id = getOrGetLastInBlock(value, AST_EXPR_IDENTIFIER)) {
      ctx.currentScope().raii.forget(id->as<Identifier>()->value);
    }

    ctx.clearScopes(true);
    ctx.ir_builder->CreateRet(val);
  } else {
    ctx.clearScopes(true);
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

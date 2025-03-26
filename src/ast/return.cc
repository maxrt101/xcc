#include "xcc/ast/return.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

Return::Return(std::shared_ptr<Node> value)
  : Node(AST_RETURN), value(std::move(value)) {}

std::shared_ptr<Return> Return::create(std::shared_ptr<Node> value) {
  return std::make_shared<Return>(std::move(value));
}

llvm::Value * Return::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  llvm::Value * val = nullptr;

  if (value) {
    val = value->generateValue(ctx, {});

    if (auto fn = ctx.globalContext.getCurrentFunction()) {
      val = codegen::castIfNotSame(ctx, val, fn->getLLVMReturnType(ctx));
    }

    ctx.ir_builder->CreateRet(val);
  } else {
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

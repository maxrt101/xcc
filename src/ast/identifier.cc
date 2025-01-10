#include "xcc/ast/identifier.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc;
using namespace xcc::ast;

Identifier::Identifier(std::string value) : Node(AST_EXPR_IDENTIFIER), value(std::move(value)) {}

std::shared_ptr<Identifier> Identifier::create(const std::string& value) {
  return std::make_shared<Identifier>(value);
}

llvm::Value * Identifier::generateValue(codegen::ModuleContext& ctx, void * payload) {
  if (ctx.hasLocal(value)) {
    return ctx.ir_builder->CreateLoad(ctx.getLocalValue(value)->getAllocatedType(), ctx.getLocalValue(value), value.c_str());
  } else if (ctx.globalContext.hasGlobal(value)) {
    auto global = ctx.globalContext.getGlobal(ctx, value);

    if (ctx.globalContext.getGlobalType(value)->isPointer()) {
      auto alloca = ctx.ir_builder->CreateAlloca(ctx.globalContext.getGlobalType(value)->getLLVMType(ctx), nullptr);
      ctx.ir_builder->CreateStore(global, alloca);
      return ctx.ir_builder->CreateLoad(ctx.globalContext.getGlobalType(value)->getLLVMType(ctx), alloca);
    }

    return ctx.ir_builder->CreateLoad(ctx.globalContext.getGlobalType(value)->getLLVMType(ctx), global);
  }
  throw CodegenException("Undeclared value referenced: '" + value + "'");
}

llvm::Value * Identifier::generateValueWithoutLoad(codegen::ModuleContext& ctx, void * payload) {
  if (ctx.hasLocal(value)) {
    return ctx.getLocalValue(value);
  } else if (ctx.globalContext.hasGlobal(value)) {
    return ctx.globalContext.getGlobal(ctx, value);
  }
  throw CodegenException("Undeclared value referenced: '" + value + "'");
}

std::shared_ptr<meta::Type> Identifier::generateType(codegen::ModuleContext& ctx, void * payload) {
  return generateTypeForValueWithoutLoad(ctx, payload);
}

std::shared_ptr<xcc::meta::Type> Identifier::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, void * payload) {
 if (ctx.hasLocal(value)) {
    return ctx.getLocalType(value);
  } else if (ctx.globalContext.hasGlobal(value)) {
    return ctx.globalContext.getGlobalType(value);
  }
  throw CodegenException("Undeclared value referenced: '" + value + "'");
}

#include "xcc/ast/identifier.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc;
using namespace xcc::ast;

Identifier::Identifier(std::string value, std::vector<std::string> scope)
  : Node(AST_EXPR_IDENTIFIER), value(std::move(value)), scope(std::move(scope)) {}

std::shared_ptr<Identifier> Identifier::create(const std::string& value, std::vector<std::string> scope) {
  return std::make_shared<Identifier>(value, scope);
}

std::shared_ptr<Node> Identifier::clone() {
  return withAttrs(create(value, scope));
}

void Identifier::visit(Visitor visitor) {}

std::string Identifier::name() const {
  std::string prefix;

  for (auto& part : scope) {
    prefix += part + "_";
  }

  return prefix + value;
}

llvm::Value * Identifier::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  if (ctx.hasLocal(name())) {
    return ctx.ir_builder->CreateLoad(ctx.getLocalValue(name())->getAllocatedType(), ctx.getLocalValue(name()), name().c_str());
  }

  if (ctx.globalContext.hasGlobal(name())) {
    auto global = ctx.globalContext.getGlobal(ctx, name());

    if (ctx.globalContext.getGlobalType(name())->isPointer()) {
      auto alloca = ctx.ir_builder->CreateAlloca(ctx.globalContext.getGlobalType(name())->getLLVMType(ctx), nullptr);
      ctx.ir_builder->CreateStore(global, alloca);
      return ctx.ir_builder->CreateLoad(ctx.globalContext.getGlobalType(name())->getLLVMType(ctx), alloca);
    }

    return ctx.ir_builder->CreateLoad(ctx.globalContext.getGlobalType(name())->getLLVMType(ctx), global);
  }

  if (auto * fn = ctx.getFunction(name())) {
    return fn;
  }

  throw CodegenException("Undeclared value referenced: '" + name() + "'");
}

llvm::Value * Identifier::generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  if (ctx.hasLocal(name())) {
    return ctx.getLocalValue(name());
  }

  if (ctx.globalContext.hasGlobal(name())) {
    return ctx.globalContext.getGlobal(ctx, name());
  }

  if (auto * fn = ctx.getFunction(name())) {
    return fn;
  }

  throw CodegenException("Undeclared value referenced: '" + name() + "'");
}

std::shared_ptr<meta::Type> Identifier::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  return generateTypeForValueWithoutLoad(ctx, payload);
}

std::shared_ptr<xcc::meta::Type> Identifier::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
 if (ctx.hasLocal(name())) {
    return ctx.getLocalType(name());
  }

  if (ctx.globalContext.hasGlobal(name())) {
    return ctx.globalContext.getGlobalType(name());
  }

  if (auto meta_fn = ctx.globalContext.getMetaFunction(name())) {
    return meta_fn->decl->generateType(ctx, payload);
  }

  throw CodegenException("Undeclared value referenced: '" + name() + "'");
}

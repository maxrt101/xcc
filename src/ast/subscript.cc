#include "xcc/ast/subscript.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;

Subscript::Subscript(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
  : Node(AST_EXPR_SUBSCRIPT), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

std::shared_ptr<Subscript> Subscript::create(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs) {
  return std::make_shared<Subscript>(std::move(lhs), std::move(rhs));
}

llvm::Value * Subscript::generateValue(codegen::ModuleContext& ctx, void * payload) {
  auto base_type = throwIfNull(lhs->generateType(ctx), CodegenException("LHS Type is NULL"));
  auto element_ptr = generateValueWithoutLoad(ctx, payload);

  return ctx.ir_builder->CreateLoad(base_type->getPointedType()->getLLVMType(ctx), element_ptr, "element");
}

llvm::Value * Subscript::generateValueWithoutLoad(codegen::ModuleContext& ctx, void * payload) {
  auto base_type = throwIfNull(lhs->generateType(ctx), CodegenException("LHS Type is NULL"));
  auto index_type = throwIfNull(rhs->generateType(ctx), CodegenException("RHS Type is NULL"));

  assertThrow(base_type->isPointer(), CodegenException("Type '" + base_type->toString() + "' is not subscriptable"));
  assertThrow(index_type->isInteger(), CodegenException("Type '" + index_type->toString() + "' is not valid for subscript index"));

  auto base_ptr = throwIfNull(lhs->generateValue(ctx), CodegenException("LHS Value is NULL"));
  auto index = throwIfNull(rhs->generateValue(ctx), CodegenException("RHS Value is NULL"));

  return ctx.ir_builder->CreateGEP(base_type->getPointedType()->getLLVMType(ctx), base_ptr, index, "element_ptr");
}

std::shared_ptr<xcc::meta::Type> Subscript::generateType(codegen::ModuleContext& ctx, void * payload) {
  auto base_type = throwIfNull(lhs->generateType(ctx), CodegenException("LHS Type is NULL"));
  return base_type->getPointedType();
}

std::shared_ptr<xcc::meta::Type> Subscript::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, void * payload) {
  return generateType(ctx, payload);
}

#include "xcc/ast/member.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;
using namespace xcc;

MemberAccess::MemberAccess(MemberAccessKind kind, std::shared_ptr<Node> lhs, std::shared_ptr<Identifier> rhs)
  : Node(AST_EXPR_MEMBER_ACCESS), kind(kind), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

std::shared_ptr<MemberAccess> MemberAccess::createByValue(std::shared_ptr<Node> lhs, std::shared_ptr<Identifier> rhs) {
  return std::make_shared<MemberAccess>(MEMBER_ACCESS_VALUE, std::move(lhs), std::move(rhs));
}

std::shared_ptr<MemberAccess> MemberAccess::createByPointer(std::shared_ptr<Node> lhs, std::shared_ptr<Identifier> rhs) {
  return std::make_shared<MemberAccess>(MEMBER_ACCESS_POINTER, std::move(lhs), std::move(rhs));
}

llvm::Value * MemberAccess::generateValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  auto type = lhs->generateTypeForValueWithoutLoad(ctx, payload);

  if (kind == MEMBER_ACCESS_POINTER) {
    assertThrow(type->isPointer(), CodegenException("Can't use '->' on a non-pointer type"));
    type = type->getPointedType();
  }

  assertThrow(type->hasMember(rhs->value), CodegenException("Type '" + type->toString() + "' doesn't have member '" + rhs->value + "'"));

  llvm::Value * value_to_load;

  if (kind == MEMBER_ACCESS_VALUE) {
    value_to_load = lhs->generateValueWithoutLoad(ctx, {});
  } else {
    value_to_load = ctx.ir_builder->CreateLoad(meta::Type::createPointer(type)->getLLVMType(ctx), lhs->generateValueWithoutLoad(ctx, payload));
  }

  return ctx.ir_builder->CreateStructGEP(type->getLLVMType(ctx), value_to_load, type->getMemberIndex(rhs->value));
}

std::shared_ptr<xcc::meta::Type> MemberAccess::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  auto type = lhs->generateTypeForValueWithoutLoad(ctx, payload);

  if (kind == MEMBER_ACCESS_POINTER) {
    assertThrow(type->isPointer(), CodegenException("Can't use '->' on a non-pointer type"));
    type = type->getPointedType();
  }

  assertThrow(type->hasMember(rhs->value), CodegenException("Type '" + type->toString() + "' doesn't have member '" + rhs->value + "'"));

  return type->getMemberType(rhs->value);
}

llvm::Value * MemberAccess::generateValue(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  if (kind == MEMBER_ACCESS_VALUE) {
    return ctx.ir_builder->CreateLoad(generateTypeForValueWithoutLoad(ctx, payload)->getLLVMType(ctx), generateValueWithoutLoad(ctx, {}), "member");
  } else {
    return generateValueWithoutLoad(ctx, payload);
  }
}

std::shared_ptr<xcc::meta::Type> MemberAccess::generateType(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  return generateTypeForValueWithoutLoad(ctx, payload);
}

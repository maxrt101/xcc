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

std::shared_ptr<Node> MemberAccess::clone() {
  return withAttrs(std::make_shared<MemberAccess>(kind, lhs->clone(), cast<Identifier>(rhs->clone())));
}

llvm::Value * MemberAccess::generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  auto type = lhs->generateTypeForValueWithoutLoad(ctx, payload);

  if (kind == MEMBER_ACCESS_POINTER) {
    assertThrow(type->isPointer(), CodegenException("Can't use '->' on a non-pointer type"));
    type = type->getPointedType();
  }

  assertThrow(type->hasMember(rhs->name()), CodegenException("Type '" + type->toString() + "' doesn't have member '" + rhs->name() + "'"));

  llvm::Value * value_to_load;

  if (kind == MEMBER_ACCESS_VALUE) {
    value_to_load = lhs->generateValueWithoutLoad(ctx, {});
  } else {
    value_to_load = ctx.ir_builder->CreateLoad(meta::Type::createPointer(type)->getLLVMType(ctx), lhs->generateValueWithoutLoad(ctx, payload));
  }

  return ctx.ir_builder->CreateStructGEP(type->getLLVMType(ctx), value_to_load, type->getMemberIndex(rhs->name()));
}

std::shared_ptr<xcc::meta::Type> MemberAccess::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  auto type = lhs->generateTypeForValueWithoutLoad(ctx, payload);

  if (kind == MEMBER_ACCESS_POINTER) {
    assertThrow(type->isPointer(), CodegenException("Can't use '->' on a non-pointer type"));
    type = type->getPointedType();
  }

  assertThrow(type->hasMember(rhs->name()), CodegenException("Type '" + type->toString() + "' doesn't have member '" + rhs->name() + "'"));

  return type->getMemberType(rhs->name());
}

llvm::Value * MemberAccess::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  llvm::Value * member_addr = generateValueWithoutLoad(ctx, payload);
  llvm::Type *  member_type = generateTypeForValueWithoutLoad(ctx, payload)->getLLVMType(ctx);

  return ctx.ir_builder->CreateLoad(member_type, member_addr, "member_value");
}

std::shared_ptr<xcc::meta::Type> MemberAccess::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  return generateTypeForValueWithoutLoad(ctx, payload);
}

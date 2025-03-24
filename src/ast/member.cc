#include "xcc/ast/member.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;
using namespace xcc;

MemberAccess::MemberAccess(std::shared_ptr<Node> lhs, std::shared_ptr<Identifier> rhs)
  : Node(AST_EXPR_MEMBER_ACCESS), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

std::shared_ptr<MemberAccess> MemberAccess::create(std::shared_ptr<Node> lhs, std::shared_ptr<Identifier> rhs) {
  return std::make_shared<MemberAccess>(std::move(lhs), std::move(rhs));
}

llvm::Value * MemberAccess::generateValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  auto type = lhs->generateTypeForValueWithoutLoad(ctx, {});

  assertThrow(type->hasMember(rhs->value), CodegenException("Type '" + type->toString() + "' doesn't have member '" + rhs->value + "'"));

  return ctx.ir_builder->CreateStructGEP(type->getLLVMType(ctx), lhs->generateValueWithoutLoad(ctx, {}), type->getMemberIndex(rhs->value));
}

std::shared_ptr<xcc::meta::Type> MemberAccess::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  auto type = lhs->generateTypeForValueWithoutLoad(ctx, {});

  assertThrow(type->hasMember(rhs->value), CodegenException("Type '" + type->toString() + "' doesn't have member '" + rhs->value + "'"));

  return type->getMemberType(rhs->value);
}

llvm::Value * MemberAccess::generateValue(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  return ctx.ir_builder->CreateLoad(generateTypeForValueWithoutLoad(ctx, {})->getLLVMType(ctx), generateValueWithoutLoad(ctx, {}), "member");
}

std::shared_ptr<xcc::meta::Type> MemberAccess::generateType(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  return generateTypeForValueWithoutLoad(ctx, payload);
}

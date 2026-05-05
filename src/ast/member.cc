#include "xcc/ast/member.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;
using namespace xcc;

MemberAccess::MemberAccess(SourceSpan span, MemberAccessKind kind, std::shared_ptr<Node> lhs, std::shared_ptr<Identifier> rhs)
  : Node(AST_EXPR_MEMBER_ACCESS, span), kind(kind), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

std::shared_ptr<MemberAccess> MemberAccess::createByValue(SourceSpan span, std::shared_ptr<Node> lhs, std::shared_ptr<Identifier> rhs) {
  return std::make_shared<MemberAccess>(span, MEMBER_ACCESS_VALUE, std::move(lhs), std::move(rhs));
}

std::shared_ptr<MemberAccess> MemberAccess::createByPointer(SourceSpan span, std::shared_ptr<Node> lhs, std::shared_ptr<Identifier> rhs) {
  return std::make_shared<MemberAccess>(span, MEMBER_ACCESS_POINTER, std::move(lhs), std::move(rhs));
}

std::shared_ptr<Node> MemberAccess::clone() {
  return withAttrs(std::make_shared<MemberAccess>(span, kind, lhs->clone(), cast<Identifier>(rhs->clone())));
}

void MemberAccess::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(lhs, visitor, ignoreSubtree);
  callVisitor(rhs, visitor, ignoreSubtree);
}

std::string MemberAccess::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(0, false) + std::format("{}{}{}",
    lhs->toString(parent, this, indent, false),
    kind == MEMBER_ACCESS_POINTER ? "->" : ".",
    rhs->toString(parent, this, indent, false)
  );
}

llvm::Value * MemberAccess::generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  auto type = lhs->generateTypeForValueWithoutLoad(ctx, payload);

  if (kind == MEMBER_ACCESS_POINTER) {
    assertRaise(type->isPointer(), Error(ERROR_POINTER_ACCESS_ON_SCALAR, span, "'{}'", type->toString()));
    type = type->getPointedType();
  }

  if (!type->isStruct()) {
    Error(ERROR_INVALID_TYPE, span, "Type '{}' is not a struct", type->toString()).raise();
  }

  assertRaise(type->hasMember(rhs->name()), Error(ERROR_UNKNOWN_MEMBER, rhs->span, "type={} member={}", type->toString(), rhs->name()));

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
    assertRaise(type->isPointer(), Error(ERROR_POINTER_ACCESS_ON_SCALAR, span, "'{}'", type->toString()));
    type = type->getPointedType();
  }

  assertRaise(type->hasMember(rhs->name()), Error(ERROR_UNKNOWN_MEMBER, rhs->span, "type={} member={}", type->toString(), rhs->name()));

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

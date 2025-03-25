#include "xcc/ast/member.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;
using namespace xcc;

// class DebugMemberPayload : public Node::Payload {
// public:
//   bool print = false;
//   bool indent = false;
//
// public:
//   explicit DebugMemberPayload(bool print, bool indent) : Node::Payload(AST_EXPR_MEMBER_ACCESS), print(print), indent(indent) {}
//
//   static std::shared_ptr<Node::Payload> create(bool print, bool indent) {
//     return std::dynamic_pointer_cast<Node::Payload>(
//       std::make_shared<DebugMemberPayload>(print, indent)
//     );
//   }
// };

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

  // auto pld = selectPayloadFirst(payload);
  //
  // auto indent = [&pld]() -> const char * {
  //   if (pld && pld->as<DebugMemberPayload>()->indent) {
  //     return "  ";
  //   }
  //   return "";
  // };
  //
  // if ((pld && pld->as<DebugMemberPayload>()->print) || !pld) {
  //   printf("%sMemberAccess::generateValueWithoutLoad(%s) %s\n",
  //     indent(),
  //     kind == MEMBER_ACCESS_VALUE ? "VALUE" : "POINTER",
  //     type->toString().c_str()
  //   );
  // }

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

  // auto pld = selectPayloadFirst(payload);
  //
  // auto indent = [&pld]() -> const char * {
  //   if (pld && pld->as<DebugMemberPayload>()->indent) {
  //     return "  ";
  //   }
  //   return "";
  // };
  //
  // if ((pld && pld->as<DebugMemberPayload>()->print) || !pld) {
  //   printf("%sMemberAccess::generateTypeForValueWithoutLoad(%s) %s\n",
  //     indent(),
  //     kind == MEMBER_ACCESS_VALUE ? "VALUE" : "POINTER",
  //     type->toString().c_str()
  //   );
  // }

  return type->getMemberType(rhs->value);
}

llvm::Value * MemberAccess::generateValue(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  // printf("MemberAccess::generateValue(%s) %s\n",
  //   kind == MEMBER_ACCESS_VALUE ? "VALUE" : "POINTER",
  //   lhs->generateTypeForValueWithoutLoad(ctx, {DebugMemberPayload::create(false, false)})->toString().c_str()
  // );

  if (kind == MEMBER_ACCESS_VALUE) {
    // printf("MemberAccess::generateValue(VALUE)\n");
    return ctx.ir_builder->CreateLoad(generateTypeForValueWithoutLoad(ctx, payload)->getLLVMType(ctx), generateValueWithoutLoad(ctx, {}), "member");
  } else {
    // printf("MemberAccess::generateValue(POINTER)\n");
    // TODO: ???
    return generateValueWithoutLoad(ctx, payload);
  }
}

std::shared_ptr<xcc::meta::Type> MemberAccess::generateType(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  // printf("MemberAccess::generateType(%s) %s\n",
  //   kind == MEMBER_ACCESS_VALUE ? "VALUE" : "POINTER",
  //   lhs->generateTypeForValueWithoutLoad(ctx, {DebugMemberPayload::create(false, false)})->toString().c_str()
  // );

  return generateTypeForValueWithoutLoad(ctx, payload);
}

#include "xcc/ast/binary.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;

Binary::Binary(Token operation, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
  : Node(AST_EXPR_BINARY), operation(std::move(operation)), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

std::shared_ptr<Binary> Binary::create(Token operation, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs) {
  return std::make_shared<Binary>(std::move(operation), std::move(lhs), std::move(rhs));
}

llvm::Value * Binary::generateValue(codegen::ModuleContext& ctx) {
  auto lhs_type = throwIfNull(lhs->generateType(ctx), CodegenException("LHS Type is NULL"));
  auto lhs_val = throwIfNull(lhs->generateValue(ctx), CodegenException("LHS Value is NULL"));

  auto rhs_type = throwIfNull(rhs->generateType(ctx), CodegenException("RHS Type is NULL"));
  auto rhs_val = throwIfNull(rhs->generateValue(ctx), CodegenException("RHS Value is NULL"));

  auto common_type = meta::Type::alignTypes(lhs_type, rhs_type);

  lhs_val = codegen::castIfNotSame(ctx, lhs_val, common_type->getLLVMType(ctx));
  rhs_val = codegen::castIfNotSame(ctx, rhs_val, common_type->getLLVMType(ctx));

  switch (operation.type) {
    case TOKEN_PLUS:
      if (common_type->isInteger()) {
        return ctx.ir_builder->CreateAdd(lhs_val, rhs_val, "addtmp");
      } else if (common_type->isFloat()) {
        return ctx.ir_builder->CreateFAdd(lhs_val, rhs_val, "addftmp");
      }
      break;

    case TOKEN_MINUS:
      if (common_type->isInteger()) {
        return ctx.ir_builder->CreateSub(lhs_val, rhs_val, "subtmp");
      } else if (common_type->isFloat()) {
        return ctx.ir_builder->CreateFSub(lhs_val, rhs_val, "subftmp");
      }
      break;

    case TOKEN_STAR:
      if (common_type->isInteger()) {
        return ctx.ir_builder->CreateMul(lhs_val, rhs_val, "multmp");
      } else if (common_type->isFloat()) {
        return ctx.ir_builder->CreateFMul(lhs_val, rhs_val, "mulftmp");
      }
      break;

    case TOKEN_SLASH:
      if (common_type->isInteger()) {
        if (common_type->isSigned()) {
          return ctx.ir_builder->CreateSDiv(lhs_val, rhs_val, "divstmp");
        } else {
          return ctx.ir_builder->CreateUDiv(lhs_val, rhs_val, "divutmp");
        }
      } else if (common_type->isFloat()) {
        return ctx.ir_builder->CreateFDiv(lhs_val, rhs_val, "divftmp");
      }
      break;

    case TOKEN_EQUALS_EQUALS:
      if (common_type->isInteger()) {
        return ctx.ir_builder->CreateICmpEQ(lhs_val, rhs_val, "cmptmp");
      } else if (common_type->isFloat()) {
        return ctx.ir_builder->CreateFCmpUEQ(lhs_val, rhs_val, "cmpftmp");
      }
      break;

    case TOKEN_NOT_EQUALS:
      if (common_type->isInteger()) {
        return ctx.ir_builder->CreateICmpNE(lhs_val, rhs_val, "cmptmp");
      } else if (common_type->isFloat()) {
        return ctx.ir_builder->CreateFCmpUNE(lhs_val, rhs_val, "cmpftmp");
      }
      break;

    case TOKEN_GREATER_EQUALS:
      if (common_type->isInteger()) {
        return ctx.ir_builder->CreateICmpUGE(lhs_val, rhs_val, "cmptmp");
      } else if (common_type->isFloat()) {
        return ctx.ir_builder->CreateFCmpUGE(lhs_val, rhs_val, "cmpftmp");
      }
      break;

    case TOKEN_GREATER:
      if (common_type->isInteger()) {
        return ctx.ir_builder->CreateICmpUGT(lhs_val, rhs_val, "cmptmp");
      } else if (common_type->isFloat()) {
        return ctx.ir_builder->CreateFCmpUGT(lhs_val, rhs_val, "cmpftmp");
      }
      break;

    case TOKEN_LESS_EQUALS:
      if (common_type->isInteger()) {
        return ctx.ir_builder->CreateICmpULE(lhs_val, rhs_val, "cmptmp");
      } else if (common_type->isFloat()) {
        return ctx.ir_builder->CreateFCmpULE(lhs_val, rhs_val, "cmpftmp");
      }
      break;

    case TOKEN_LESS:
      if (common_type->isInteger()) {
        return ctx.ir_builder->CreateICmpULT(lhs_val, rhs_val, "cmptmp");
      } else if (common_type->isFloat()) {
        return ctx.ir_builder->CreateFCmpULT(lhs_val, rhs_val, "cmpftmp");
      }
      break;

    default:
      break;
  }

  // TODO: Make Type::tagToString()
  throw CodegenException(operation.line, "Unsupported binary expression operator or type (op='" + operation.value + "' " + Token::typeToString(operation.type) + " type=" + std::to_string((int)common_type->getTag()) + ")");
}

std::shared_ptr<xcc::meta::Type> Binary::generateType(codegen::ModuleContext& ctx) {
  auto lhs_type = throwIfNull(lhs->generateType(ctx), CodegenException("LHS type is NULL"));
  auto rhs_type = throwIfNull(rhs->generateType(ctx), CodegenException("RHS type is NULL"));

  return meta::Type::alignTypes(lhs_type, rhs_type);
}

#include "xcc/ast/unary.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;

Unary::Unary(Token operation, std::shared_ptr<Node> rhs)
    : Node(AST_EXPR_UNARY), operation(std::move(operation)), rhs(std::move(rhs)) {}

std::shared_ptr<Unary> Unary::create(Token operation, std::shared_ptr<Node> rhs) {
  return std::make_shared<Unary>(std::move(operation), std::move(rhs));
}

llvm::Value * Unary::generateValue(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  switch (operation.type) {
    case TOKEN_STAR: {
      return ctx.ir_builder->CreateLoad(generateTypeForValueWithoutLoad(ctx, {})->getLLVMType(ctx), generateValueWithoutLoad(ctx, {}), "dereferenced");
    }

    default:
      return generateValueWithoutLoad(ctx, {});
  }
}

llvm::Value * Unary::generateValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  auto rhs_type = throwIfNull(rhs->generateType(ctx, {}), CodegenException("RHS Type is NULL"));
  auto rhs_val = throwIfNull(rhs->generateValue(ctx, {}), CodegenException("RHS Value is NULL"));

  switch (operation.type) {
    case TOKEN_AMP: {
      assertThrow(rhs->is(ast::AST_EXPR_IDENTIFIER), CodegenException("Invalid RHS node for unary operator '&'"));

      auto identifier = rhs->as<ast::Identifier>();

      return identifier->generateValueWithoutLoad(ctx, {});
    }

    case TOKEN_STAR: {
      if (!rhs_type->isPointer()) {
        throw CodegenException(operation.line, "Value is not a pointer (unary '*' operator)");
      }
      return rhs_val;
    }

    default:
      break;
  }

  throw CodegenException(operation.line, "Unsupported unary expression operator/type or can't generate without load (op='" + operation.value + "' " + Token::typeToString(operation.type) + " type=" + std::to_string((int)rhs_type->getTag()) + ")");
}

std::shared_ptr<xcc::meta::Type> Unary::generateType(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  return throwIfNull(rhs->generateType(ctx, {}), CodegenException("RHS Type is NULL"));
}

std::shared_ptr<xcc::meta::Type> Unary::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  return throwIfNull(rhs->generateType(ctx, {})->getPointedType(), CodegenException("RHS Type is NULL"));
}

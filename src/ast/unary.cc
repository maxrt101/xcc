#include "xcc/ast/unary.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;

Unary::Unary(SourceSpan span, Token operation, std::shared_ptr<Node> rhs)
    : Node(AST_EXPR_UNARY, span), operation(std::move(operation)), rhs(std::move(rhs)) {}

std::shared_ptr<Unary> Unary::create(SourceSpan span, Token operation, std::shared_ptr<Node> rhs) {
  return std::make_shared<Unary>(span, std::move(operation), std::move(rhs));
}

std::shared_ptr<Node> Unary::clone() {
  return withAttrs(create(span, operation, rhs->clone()));
}

void Unary::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(rhs, visitor, ignoreSubtree);
}

std::string Unary::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return operation.toString() + rhs->toString(parent, this, indent, false);
}

llvm::Value * Unary::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  switch (operation.type) {
    case TOKEN_STAR: {
      return ctx.ir_builder->CreateLoad(generateTypeForValueWithoutLoad(ctx, payload)->getLLVMType(ctx), generateValueWithoutLoad(ctx, {}), "dereferenced");
    }

    default:
      return generateValueWithoutLoad(ctx, {});
  }
}

llvm::Value * Unary::generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  auto rhs_type = raiseIfNull(rhs->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, rhs->span, "RHS Type is NULL"));

  switch (operation.type) {
    case TOKEN_AMP: {
      assertRaise(rhs->is(ast::AST_EXPR_IDENTIFIER), Error(ERROR_INVALID_UNARY_AMP_RHS, rhs->span, ""));

      auto identifier = rhs->as<ast::Identifier>();

      return identifier->generateValueWithoutLoad(ctx, payload);
    }

    case TOKEN_STAR: {
      if (!rhs_type->isPointer()) {
        Error(ERROR_INVALID_UNARY_STAR_RHS, rhs->span, "").raise();
      }
      return raiseIfNull(rhs->generateValue(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, rhs->span, "RHS Value is NULL"));
    }

    default:
      break;
  }

  Error(ERROR_UNKNOWN_UNARY_OP_OR_TYPE, operation.span, "op='{}'({}) type={}", operation.value, Token::typeToString(operation.type), std::to_string((int)rhs_type->getTag())).raise();
}

std::shared_ptr<xcc::meta::Type> Unary::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  /* If dereferencing - should return TypeForValueWithoutLoad */
  return operation.type == TOKEN_STAR
      ? generateTypeForValueWithoutLoad(ctx, payload)
      : raiseIfNull(rhs->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, rhs->span, "RHS Type is NULL"));
}

std::shared_ptr<xcc::meta::Type> Unary::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  auto rhs_type = raiseIfNull(rhs->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, rhs->span, "RHS Type is NULL"));

  if (!rhs_type->isPointer()) {
    Error(ERROR_INVALID_UNARY_STAR_RHS, rhs->span, "").raise();
  }

  return rhs_type->getPointedType();
}

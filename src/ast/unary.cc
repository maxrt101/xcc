#include "xcc/ast/unary.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;

Unary::Unary(SourceSpan span, LexicalScope scope, Token operation, std::shared_ptr<Node> rhs)
    : Node(AST_EXPR_UNARY, span, scope), operation(std::move(operation)), rhs(std::move(rhs)) {}

std::shared_ptr<Unary> Unary::create(SourceSpan span, LexicalScope scope, Token operation, std::shared_ptr<Node> rhs) {
  return std::make_shared<Unary>(span, scope, std::move(operation), std::move(rhs));
}

std::shared_ptr<Node> Unary::clone() {
  return withAttrs(create(span, scope, operation, rhs->clone()));
}

void Unary::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(globalContext, rhs, visitor, ignoreSubtree);
}

std::string Unary::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(0, false) + operation.toString() + rhs->toString(parent, this, indent, false);
}

llvm::Value * Unary::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  switch (operation.type) {
    case TOKEN_STAR: {
      return ctx.ir_builder->CreateLoad(generateTypeForValueWithoutLoad(ctx, payload)->getLLVMType(ctx), generateValueWithoutLoad(ctx, payload), "dereferenced");
    }

    default:
      return generateValueWithoutLoad(ctx, payload);
  }
}

llvm::Value * Unary::generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  auto rhs_type = raiseIfNull(rhs->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, rhs->span, "RHS Type is NULL"));

  switch (operation.type) {
    case TOKEN_AMP: {
      return rhs->generateValueWithoutLoad(ctx, payload);
    }

    case TOKEN_STAR: {
      if (!rhs_type->isPointer()) {
        Error(ERROR_INVALID_UNARY_STAR_RHS, rhs->span).raiseFromNode(this);
      }
      return raiseIfNull(rhs->generateValue(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, rhs->span, "RHS Value is NULL"));
    }

    case TOKEN_NOT: {
      auto val = rhs->generateValue(ctx, payload);
      val = castIfNotSame(ctx, val, meta::Type::createBool()->getLLVMType(ctx), span);
      return ctx.ir_builder->CreateNot(val, "b_not");
    }

    case TOKEN_MINUS: {
      return ctx.ir_builder->CreateNeg(rhs->generateValue(ctx, payload), "neg");
    }

    case TOKEN_TILDA: {
      return ctx.ir_builder->CreateNot(rhs->generateValue(ctx, payload), "not");
    }

    default:
      break;
  }

  Error(ERROR_UNKNOWN_UNARY_OP_OR_TYPE, operation.span, "op='{}'({}) type={}", operation.value, Token::typeToString(operation.type), std::to_string((int)rhs_type->getTag())).raiseFromNode(this);
}

llvm::Constant * Unary::generateConstant(codegen::ModuleContext& ctx, PayloadList payload) {
  auto rhs_type = raiseIfNull(rhs->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, rhs->span, "RHS Type is NULL"));

  auto val = rhs->generateConstant(ctx, payload);
  auto s   = llvm::dyn_cast<llvm::ConstantInt>(val);
  auto t   = llvm::IntegerType::get(*ctx.llvm.ctx, rhs_type->getNumberBitWidth());

  assertRaiseFromNode(s, Error(ERROR_UNARY_RHS_NOT_CONSTANT, rhs->span), this);

  switch (operation.type) {
    case TOKEN_NOT:   return llvm::ConstantInt::get(t, !s->getValue());
    case TOKEN_MINUS: return llvm::ConstantInt::get(t, -s->getValue());
    case TOKEN_TILDA: return llvm::ConstantInt::get(t, ~s->getValue());
    default:
      break;
  }

  Error(ERROR_UNKNOWN_UNARY_OP_OR_TYPE, operation.span, "op='{}'({}) type={}", operation.value, Token::typeToString(operation.type), std::to_string((int)rhs_type->getTag())).raiseFromNode(this);
}

std::shared_ptr<xcc::meta::Type> Unary::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  /* If dereferencing - should return TypeForValueWithoutLoad */
  if (operation.type == TOKEN_STAR) {
    return generateTypeForValueWithoutLoad(ctx, payload);
  }

  auto t = raiseIfNull(rhs->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, rhs->span, "RHS Type is NULL"));

  if (operation.type == TOKEN_AMP) {
    t = meta::Type::createPointer(t);
  }

  return t;
}

std::shared_ptr<xcc::meta::Type> Unary::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  auto rhs_type = raiseIfNull(rhs->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, rhs->span, "RHS Type is NULL"));

  if (!rhs_type->isPointer()) {
    Error(ERROR_INVALID_UNARY_STAR_RHS, rhs->span).raiseFromNode(this);
  }

  return rhs_type->getPointedType();
}

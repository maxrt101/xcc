#include "xcc/ast/assign.h"
#include "xcc/ast/unary.h"
#include "xcc/ast/subscript.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;
using namespace xcc;

Assign::Assign(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
  : Node(AST_EXPR_ASSIGN), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

std::shared_ptr<Assign> Assign::create(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs) {
  return std::make_shared<Assign>(std::move(lhs), std::move(rhs));
}

llvm::Value * Assign::generateValue(codegen::ModuleContext& ctx) {
  auto val = throwIfNull(rhs->generateValue(ctx), CodegenException("assignment value generated NULL"));

  // FIXME: What to do with edge cases? `*ptr = val` and `arr[idx] = val`
  if (lhs->is(ast::AST_EXPR_IDENTIFIER)) {
    auto name = ast::Node::cast<ast::Identifier>(lhs);

    if (ctx.namedValues.find(name->value) != ctx.namedValues.end()) {
      val = codegen::castIfNotSame(ctx, val, ctx.namedValues[name->value]->type->getLLVMType(ctx));

      ctx.ir_builder->CreateStore(val, ctx.namedValues[name->value]->value);
      return val;
    }

    throw CodegenException("Unknown variable '" + name->value + "'");
  } else if (lhs->is(ast::AST_EXPR_UNARY)) {
    auto unary = ast::Node::cast<ast::Unary>(lhs);

    assertThrow(unary->operation.type == TOKEN_STAR, CodegenException("Invalid LHS node for assignment"));

    auto rhs_type = throwIfNull(unary->rhs->generateType(ctx), CodegenException("Unary RHS Type is NULL"));
    auto rhs_val = throwIfNull(unary->rhs->generateValue(ctx), CodegenException("Unary RHS Value is NULL"));

    val = codegen::castIfNotSame(ctx, val, rhs_type->getPointedType()->getLLVMType(ctx));

    return ctx.ir_builder->CreateStore(val, rhs_val);
  } else if (lhs->is(ast::AST_EXPR_SUBSCRIPT)) {
    auto subscript = ast::Node::cast<ast::Subscript>(lhs);

    auto base_type = throwIfNull(subscript->lhs->generateType(ctx), CodegenException("LHS Type is NULL"));
    auto index_type = throwIfNull(subscript->rhs->generateType(ctx), CodegenException("RHS Type is NULL"));

    assertThrow(base_type->isPointer(), CodegenException("Type '" + base_type->toString() + "' is not subscriptable"));
    assertThrow(index_type->isInteger(), CodegenException("Type '" + index_type->toString() + "' is not valid for subscript index"));

    auto base_ptr = throwIfNull(subscript->lhs->generateValue(ctx), CodegenException("LHS Value is NULL"));
    auto index = throwIfNull(subscript->rhs->generateValue(ctx), CodegenException("RHS Value is NULL"));

    auto element_ptr = ctx.ir_builder->CreateGEP(base_type->getPointedType()->getLLVMType(ctx), base_ptr, index, "element_ptr");

    val = codegen::castIfNotSame(ctx, val, base_type->getPointedType()->getLLVMType(ctx));

    return ctx.ir_builder->CreateStore(val, element_ptr);
  }

  throw CodegenException("Invalid LHS node for assignment");
}

std::shared_ptr<xcc::meta::Type> Assign::generateType(codegen::ModuleContext& ctx) {
  if (lhs->is(ast::AST_EXPR_IDENTIFIER)) {
    auto name = ast::Node::cast<ast::Identifier>(lhs);

    if (ctx.namedValues.find(name->value) != ctx.namedValues.end()) {
      return ctx.namedValues[name->value]->type;
    }

    throw CodegenException("Unknown variable '" + name->value + "'");
  } else if (lhs->is(ast::AST_EXPR_UNARY)) {
    return lhs->generateType(ctx);
  } else if (lhs->is(ast::AST_EXPR_SUBSCRIPT)) {
    auto base_type = throwIfNull(lhs->generateType(ctx), CodegenException("LHS Type is NULL"));
    return base_type->getPointedType();
  }

  throw CodegenException("Invalid LHS node for assignment");
}

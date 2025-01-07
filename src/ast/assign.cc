#include "xcc/ast/assign.h"
#include "xcc/ast/unary.h"
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

    auto lhs_type = throwIfNull(unary->rhs->generateType(ctx), CodegenException("Unary RHS Type is NULL"));
    auto lhs_val = throwIfNull(unary->rhs->generateValue(ctx), CodegenException("Unary RHS Value is NULL"));

    val = codegen::castIfNotSame(ctx, val, lhs_type->getPointedType()->getLLVMType(ctx));

    return ctx.ir_builder->CreateStore(val, lhs_val);
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
  }

  throw CodegenException("Invalid LHS node for assignment");
}

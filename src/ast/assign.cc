#include "xcc/ast/assign.h"
//#include "xcc/ast/unary.h"
#include "xcc/ast/number.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;
using namespace xcc;

Assign::Assign(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
  : Node(AST_EXPR_ASSIGN), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

std::shared_ptr<Assign> Assign::create(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs) {
  return std::make_shared<Assign>(std::move(lhs), std::move(rhs));
}

llvm::Value * Assign::generateValue(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  auto val = throwIfNull(rhs->generateValue(ctx, {}), CodegenException("assignment value generated NULL"));

  val = codegen::castIfNotSame(ctx, val, lhs->generateTypeForValueWithoutLoad(ctx, {})->getLLVMType(ctx));
  return ctx.ir_builder->CreateStore(val, lhs->generateValueWithoutLoad(ctx, {}));
}

std::shared_ptr<xcc::meta::Type> Assign::generateType(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  if (lhs->is(ast::AST_EXPR_IDENTIFIER)) {
    auto name = ast::Node::cast<ast::Identifier>(lhs);

    if (ctx.hasLocal(name->value)) {
      return ctx.getLocalType(name->value);
    }

    throw CodegenException("Unknown variable '" + name->value + "'");
  } else {
    return lhs->generateTypeForValueWithoutLoad(ctx, {});
  }

  throw CodegenException("Invalid LHS node for assignment");
}

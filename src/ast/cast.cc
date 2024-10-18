#include "xcc/ast/cast.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

Cast::Cast(std::shared_ptr<Node> expr, std::shared_ptr<Type> type)
  : Node(AST_EXPR_CAST), expr(std::move(expr)), type(std::move(type)) {}

std::shared_ptr<Cast> Cast::create(std::shared_ptr<Node> expr, std::shared_ptr<Type> type) {
  return std::make_shared<Cast>(std::move(expr), std::move(type));
}

llvm::Value * Cast::generateValue(codegen::ModuleContext& ctx) {
  return codegen::castIfNotSame(ctx, expr->generateValue(ctx), type->generateType(ctx)->getLLVMType(ctx));
}

std::shared_ptr<xcc::meta::Type> Cast::generateType(codegen::ModuleContext& ctx) {
  return type->generateType(ctx);
}

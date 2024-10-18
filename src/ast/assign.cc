#include "xcc/ast/assign.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;
using namespace xcc;

Assign::Assign(std::shared_ptr<Identifier> name, std::shared_ptr<Node> value)
  : Node(AST_EXPR_ASSIGN), name(std::move(name)), value(std::move(value)) {}

std::shared_ptr<Assign> Assign::create(std::shared_ptr<Identifier> name, std::shared_ptr<Node> value) {
  return std::make_shared<Assign>(std::move(name), std::move(value));
}

llvm::Value * Assign::generateValue(codegen::ModuleContext& ctx) {
  auto val = throwIfNull(value->generateValue(ctx), CodegenException("assignment value generated NULL"));

  if (ctx.namedValues.find(name->value) != ctx.namedValues.end()) {
    val = codegen::castIfNotSame(ctx, val, ctx.namedValues[name->value]->type->getLLVMType(ctx));

    ctx.ir_builder->CreateStore(val, ctx.namedValues[name->value]->value);
    return val;
  }

  throw CodegenException("Unknown variable '" + name->value + "'");
}

std::shared_ptr<xcc::meta::Type> Assign::generateType(codegen::ModuleContext& ctx) {
  if (ctx.namedValues.find(name->value) != ctx.namedValues.end()) {
    return ctx.namedValues[name->value]->type;
  }

  throw CodegenException("Unknown variable '" + name->value + "'");
}

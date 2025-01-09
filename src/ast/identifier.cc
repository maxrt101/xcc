#include "xcc/ast/identifier.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc;
using namespace xcc::ast;

Identifier::Identifier(std::string value) : Node(AST_EXPR_IDENTIFIER), value(std::move(value)) {}

std::shared_ptr<Identifier> Identifier::create(const std::string& value) {
  return std::make_shared<Identifier>(value);
}

llvm::Value * Identifier::generateValue(codegen::ModuleContext& ctx) {
  if (ctx.namedValues.find(value) != ctx.namedValues.end()) {
    return ctx.ir_builder->CreateLoad(ctx.namedValues[value]->value->getAllocatedType(), ctx.namedValues[value]->value, value.c_str());
  }
  throw CodegenException("Undeclared value referenced: '" + value + "'");
}

std::shared_ptr<meta::Type> Identifier::generateType(codegen::ModuleContext& ctx) {
  if (ctx.namedValues.find(value) != ctx.namedValues.end()) {
    return ctx.namedValues[value]->type;
  }
  throw CodegenException("Undeclared value referenced: '" + value + "'");
}

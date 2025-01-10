#include "xcc/ast/number.h"
#include "xcc/exceptions.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

Number::Number() : Node(AST_EXPR_NUMBER) {}

std::shared_ptr<Number> Number::createInteger(int64_t value) {
  auto number = std::make_shared<Number>();
  number->tag = INTEGER;
  number->value.integer = value;
  return number;
}

std::shared_ptr<Number> Number::createFloating(double value) {
  auto number = std::make_shared<Number>();
  number->tag = FLOATING;
  number->value.floating = value;
  return number;
}

llvm::Value * Number::generateValueWithSpecificBitWidth(codegen::ModuleContext& ctx, int bits) const {
  if (tag == FLOATING) {
    return llvm::ConstantFP::get(*ctx.llvm.ctx, llvm::APFloat(value.floating));
  } else if (tag == INTEGER) {
    return llvm::ConstantInt::get(llvm::IntegerType::get(*ctx.llvm.ctx, bits), value.integer);
  }

  throw CodegenException("Invalid number literal");
}

llvm::Value * Number::generateValue(codegen::ModuleContext& ctx) {
  return generateValueWithoutLoad(ctx);
}

llvm::Value * Number::generateValueWithoutLoad(codegen::ModuleContext& ctx) {
  if (tag == FLOATING) {
    return llvm::ConstantFP::get(*ctx.llvm.ctx, llvm::APFloat(value.floating));
  } else if (tag == INTEGER) {
    return llvm::ConstantInt::get(llvm::IntegerType::get(*ctx.llvm.ctx, 64), value.integer);
  }

  throw CodegenException("Invalid number literal");
}

std::shared_ptr<xcc::meta::Type> Number::generateType(codegen::ModuleContext& ctx) {
  if (tag == FLOATING) {
    return xcc::meta::Type::createF64();
  } else if (tag == INTEGER) {
    return xcc::meta::Type::createI64();
  }

  throw CodegenException("Invalid number literal");
}

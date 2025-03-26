#include "xcc/ast/number.h"
#include "xcc/exceptions.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

Number::Payload::Payload(int bits) : Node::Payload(ast::AST_EXPR_NUMBER), bits(bits) {}

std::shared_ptr<Node::Payload> Number::Payload::create(int bits) {
  return std::dynamic_pointer_cast<Node::Payload>(
      std::make_shared<Number::Payload>(bits)
  );
}

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

llvm::Value * Number::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  return generateValueWithoutLoad(ctx, payload);
}

llvm::Value * Number::generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  int bits = 64;

  if (auto p = selectPayloadFirst(payload)) {
    bits = p->as<Number::Payload>()->bits;
  }

  if (tag == FLOATING) {
    return llvm::ConstantFP::get(
        *ctx.llvm.ctx,
        llvm::APFloat(bits == 32
            ? (float)  value.floating
            : (double) value.floating)
    );
  } else if (tag == INTEGER) {
    return llvm::ConstantInt::get(llvm::IntegerType::get(*ctx.llvm.ctx, bits), value.integer);
  }

  throw CodegenException("Invalid number literal");
}

std::shared_ptr<xcc::meta::Type> Number::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  return generateTypeForValueWithoutLoad(ctx, std::move(payload));
}

std::shared_ptr<xcc::meta::Type> Number::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  int bits = 64;

  if (auto p = selectPayloadFirst(payload)) {
    bits = p->as<Number::Payload>()->bits;
  }

  if (tag == FLOATING) {
    return xcc::meta::Type::createFloating(bits);
  } else if (tag == INTEGER) {
    return xcc::meta::Type::createSigned(bits);
  }

  throw CodegenException("Invalid number literal");
}

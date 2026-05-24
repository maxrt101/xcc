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

Number::Number(SourceSpan span, LexicalScope scope) : Node(AST_EXPR_NUMBER, span, scope) {}

std::shared_ptr<Number> Number::createInteger(SourceSpan span, LexicalScope scope, int64_t value) {
  auto number = std::make_shared<Number>(span, scope);
  number->tag = INTEGER;
  number->value.integer = value;
  return number;
}

std::shared_ptr<Number> Number::createFloating(SourceSpan span, LexicalScope scope, double value) {
  auto number = std::make_shared<Number>(span, scope);
  number->tag = FLOATING;
  number->value.floating = value;
  return number;
}

std::shared_ptr<Node> Number::clone() {
  if (tag == INTEGER) {
    return withAttrs(createInteger(span, scope, value.integer));
  }

  if (tag == FLOATING) {
    return withAttrs(createFloating(span, scope, value.floating));
  }

  Error(ERROR_INVALID_NUMBER_LITERAL, span).raiseFromNode(this);
}

void Number::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {}

std::string Number::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(0, false) + (tag == INTEGER ? std::format("{}", value.integer) : std::format("{}", value.floating));
}

llvm::Value * Number::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  return generateValueWithoutLoad(ctx, payload);
}

llvm::Value * Number::generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  return generateConstant(ctx, payload);
}

llvm::Constant * Number::generateConstant(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

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
  }

  if (tag == INTEGER) {
    return llvm::ConstantInt::get(llvm::IntegerType::get(*ctx.llvm.ctx, bits), value.integer);
  }

  Error(ERROR_INVALID_NUMBER_LITERAL, span).raiseFromNode(this);
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

  Error(ERROR_INVALID_NUMBER_LITERAL, span).raiseFromNode(this);
}

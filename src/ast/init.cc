#include "xcc/ast/init.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

Initializer::Initializer(SourceSpan span, std::shared_ptr<Node> value_type, std::vector<Value> values)
  : Node(AST_INIT, span), value_type(std::move(value_type)), values(std::move(values)) {}

std::shared_ptr<Initializer> Initializer::create(SourceSpan span, std::shared_ptr<Node> value_type, std::vector<Value> values) {
  return std::make_shared<Initializer>(span, std::move(value_type), std::move(values));
}

std::shared_ptr<Node> Initializer::clone() {
  std::vector<Value> values;

  for (auto& value : this->values) {
    values.push_back({value.name->clone(), value.value->clone()});
  }

  return withAttrs(create(span, value_type->clone(), values));
}

void Initializer::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(value_type, visitor, ignoreSubtree);

  for (auto& value : values) {
    callVisitor(value.name, visitor, ignoreSubtree);
    callVisitor(value.value, visitor, ignoreSubtree);
  }
}

std::string Initializer::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(indent, newline) + value_type->toString(parent, this, indent, false) + " {";

  for (size_t i = 0; i < values.size(); ++i) {
    if (values[i].name) {
      res += values[i].name->toString(parent, this, indent, false) + ": ";
    }
    res += values[i].value->toString(parent, this, indent, false);
    if (i + 1 < values.size()) {
      res += ", ";
    }
  }

  return res + "}";
}

llvm::Value * Initializer::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  auto t = generateType(ctx, payload);
  auto alloca = ctx.createEntryBlockAlloca(t->getLLVMType(ctx), "init");

  if (t->isStruct()) {
    fillStruct(ctx, t, alloca);
  } else if (t->isArray()) {
    fillArray(ctx, t, alloca);
  } else {
    // TODO: Raise error
  }

  return ctx.ir_builder->CreateLoad(t->getLLVMType(ctx), alloca);
}

std::shared_ptr<xcc::meta::Type> Initializer::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  auto t = value_type->generateType(ctx, payload);

  if (!t->isStruct()) {
    t = meta::Type::createArray(t, values.size());
  }

  return t;
}

void Initializer::fillStruct(codegen::ModuleContext& ctx, std::shared_ptr<meta::Type>& t, llvm::AllocaInst * alloca) {
  auto * structTy = llvm::cast<llvm::StructType>(t->getLLVMType(ctx));;

  ctx.ir_builder->CreateStore(llvm::ConstantAggregateZero::get(structTy), alloca);

  for (auto& value : values) {
    assertRaise(value.name && value.name->is(AST_EXPR_IDENTIFIER),
      Error(ERROR_INIT_EXPECTED_NAMED_VALUE, value.name ? value.name->span : value.value->span, ""));

    uint32_t fieldIdx = t->getMemberIndex(value.name->as<Identifier>()->value);

    auto * fieldPtr = ctx.ir_builder->CreateStructGEP(structTy, alloca, fieldIdx);

    auto * val = value.value->generateValue(ctx, {});

    ctx.ir_builder->CreateStore(val, fieldPtr);
  }
}

void Initializer::fillArray(codegen::ModuleContext& ctx, std::shared_ptr<meta::Type>& t, llvm::AllocaInst * alloca) {
  auto * arrayTy = t->getLLVMArrayType(ctx);

  for (size_t i = 0; i < values.size(); ++i) {
    std::vector<llvm::Value*> indices = {
      ctx.ir_builder->getInt32(0), // Offset from the base pointer
      ctx.ir_builder->getInt32(i)  // Index of the child element
    };
    auto * elementPtr = ctx.ir_builder->CreateInBoundsGEP(arrayTy, alloca, indices);
    auto * val = values[i].value->generateValue(ctx, {});
    ctx.ir_builder->CreateStore(val, elementPtr);
  }
}

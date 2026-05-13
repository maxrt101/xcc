#include "xcc/ast/init.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

Initializer::Initializer(SourceSpan span, std::shared_ptr<Node> value_type, std::vector<Value> values, bool has_square_braces)
  : Node(AST_INIT, span), value_type(std::move(value_type)), values(std::move(values)), has_square_braces(has_square_braces) {}

std::shared_ptr<Initializer> Initializer::create(SourceSpan span, std::shared_ptr<Node> value_type, std::vector<Value> values, bool has_square_braces) {
  return std::make_shared<Initializer>(span, std::move(value_type), std::move(values), has_square_braces);
}

std::shared_ptr<Node> Initializer::clone() {
  std::vector<Value> values;

  for (auto& value : this->values) {
    values.push_back({value.name->clone(), value.value->clone()});
  }

  return withAttrs(create(span, value_type->clone(), values, has_square_braces));
}

void Initializer::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(value_type, visitor, ignoreSubtree);

  for (auto& value : values) {
    callVisitor(value.name, visitor, ignoreSubtree);
    callVisitor(value.value, visitor, ignoreSubtree);
  }
}

std::string Initializer::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(indent, newline);

  bool not_normal = value_type->as<Type>()->kind != Type::NORMAL;

  if (not_normal) res += "[";

  res += value_type->toString(parent, this, indent, false);

  if (not_normal) res += "]";

  res += " {";

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
  auto alloca = generateValueWithoutLoad(ctx, payload);
  auto t      = generateType(ctx, payload);

  return ctx.ir_builder->CreateLoad(t->getLLVMType(ctx), alloca);
}

llvm::Value * Initializer::generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
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

  return alloca;
}

llvm::Constant * Initializer::generateConstant(codegen::ModuleContext& ctx, PayloadList payload) {
  auto t         = generateType(ctx, payload);
  auto llvm_type = t->getLLVMType(ctx);

  if (t->isStruct()) {
    std::vector<llvm::Constant *> fields;

    for (auto& val : values) {
      fields.push_back(val.value->generateConstant(ctx, payload));
    }

    return llvm::ConstantStruct::get(llvm::cast<llvm::StructType>(llvm_type), fields);
  }

  if (t->isArray()) {
    std::vector<llvm::Constant *> elements;

    for (auto& val : values) {
      elements.push_back(val.value->generateConstant(ctx, payload));
    }

    return llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(llvm_type), elements);
  }

  return llvm::cast<llvm::Constant>(generateValue(ctx, payload));
}

std::shared_ptr<xcc::meta::Type> Initializer::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  auto t = value_type->generateType(ctx, payload);

  // If initializer is not for a struct, it must be for an array
  if (!t->isStruct()) {
    if (t->isArray() && !t->getElementCount()) {
      // If type is an array, but has size=0, update it
      t = meta::Type::createArray(t->getElementType(), values.size());
    }
  }

  if (!t->isStruct() || has_square_braces) {
    // Implicitly wrap the type in an array, because '[i32] {}' -> type 'i32', should be 'i32[]'
    t = meta::Type::createArray(t, values.size());
  }

  return t;
}

std::shared_ptr<xcc::meta::Type> Initializer::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  return meta::Type::createPointer(generateType(ctx, payload));
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
    std::vector<llvm::Value *> indices = {
      ctx.ir_builder->getInt32(0), // Offset from the base pointer
      ctx.ir_builder->getInt32(i)  // Index of the child element
    };
    auto * elementPtr = ctx.ir_builder->CreateInBoundsGEP(arrayTy, alloca, indices);
    auto * val = castIfNotSame(ctx, values[i].value->generateValue(ctx, {}), arrayTy->getElementType(), values[i].value->span);
    ctx.ir_builder->CreateStore(val, elementPtr);
  }
}

#include "xcc/ast/init.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

Initializer::Payload::Payload(std::shared_ptr<meta::Type> type)
  : Node::Payload(AST_INIT), type(std::move(type)) {}

std::shared_ptr<Node::Payload> Initializer::Payload::create(std::shared_ptr<meta::Type> type) {
  return std::dynamic_pointer_cast<Node::Payload>(
      std::make_shared<Initializer::Payload>(std::move(type))
  );
}

Initializer::Initializer(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> value_type, std::vector<Value> values, bool has_square_braces)
  : Node(AST_INIT, span, scope), value_type(std::move(value_type)), values(std::move(values)), has_square_braces(has_square_braces) {}

std::shared_ptr<Initializer> Initializer::create(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> value_type, std::vector<Value> values, bool has_square_braces) {
  return std::make_shared<Initializer>(span, scope, std::move(value_type), std::move(values), has_square_braces);
}

std::shared_ptr<Node> Initializer::clone() {
  std::vector<Value> values;

  for (auto& value : this->values) {
    values.push_back({value.name ? value.name->clone() : nullptr, value.value->clone()});
  }

  return withAttrs(create(span, scope, value_type->clone(), values, has_square_braces));
}

void Initializer::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(globalContext, value_type, visitor, ignoreSubtree);

  for (auto& value : values) {
    callVisitor(globalContext, value.name, visitor, ignoreSubtree);
    callVisitor(globalContext, value.value, visitor, ignoreSubtree);
  }
}

std::string Initializer::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(indent, newline);

  bool not_normal = value_type ? value_type->as<Type>()->kind != Type::NORMAL : true;

  if (not_normal) res += "[";

  if (value_type) {
    res += value_type->toString(parent, this, indent, false);
  }

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

  auto t      = generateType(ctx, payload);
  auto alloca = ctx.createEntryBlockAlloca(t->getLLVMType(ctx), "init");

  if (t->isStruct()) {
    fillStruct(ctx, t, alloca, payload);
  } else if (t->isArray()) {
    fillArray(ctx, t, alloca, payload);
  } else if (t->isTuple()) {
    fillTuple(ctx, t, alloca, payload);
  } else {
    Error(ERROR_UNINITIALIZABLE_TYPE, span, "'{}'", t->toString()).raiseFromNode(this);
  }

  return alloca;
}

llvm::Constant * Initializer::generateConstant(codegen::ModuleContext& ctx, PayloadList payload) {
  auto t         = generateType(ctx, payload);
  auto llvm_type = t->getLLVMType(ctx);

  if (t->isStruct() || t->isArray() || t->isTuple()) {
    std::vector<llvm::Constant *> members;

    for (auto& val : values) {
      members.push_back(val.value->generateConstant(ctx, payload));
    }

    return t->isArray()
      ? llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(llvm_type), members)
      : llvm::ConstantStruct::get(llvm::cast<llvm::StructType>(llvm_type), members);
  }

  return llvm::cast<llvm::Constant>(generateValue(ctx, payload));
}

std::shared_ptr<xcc::meta::Type> Initializer::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  if (!value_type) {
    if (auto p = selectPayloadFirst(payload)) {
      return p->as<Initializer::Payload>()->type;
    }

    Error(ERROR_CANT_INFER_TYPE, span).raiseFromNode(this);
  }

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

void Initializer::fillTuple(codegen::ModuleContext& ctx, std::shared_ptr<meta::Type>& t, llvm::AllocaInst * alloca, PayloadList payload) {
  auto * structTy = llvm::cast<llvm::StructType>(t->getLLVMType(ctx));;

  // Zero-initialize the whole thing to have a determined state for uninitialized fields
  ctx.ir_builder->CreateStore(llvm::ConstantAggregateZero::get(structTy), alloca);

  assertRaiseFromNode(values.size() == t->getTupleMemberCount(),
    Error(ERROR_TUPLE_MISSING_FIELDS, span,
      "tuple '{}' has {} elements, initializer got {}", t->toString(), t->getTupleMemberCount(), values.size()), this);

  for (size_t i = 0; i < values.size(); ++i) {
    payload = extendPayload(excludePayload(payload, AST_INIT), Initializer::Payload::create(t->getTupleMemberType(i)));

    auto * fieldPtr = ctx.ir_builder->CreateStructGEP(structTy, alloca, i);

    auto * val = values[i].value->generateValue(ctx, payload);

    ctx.ir_builder->CreateStore(val, fieldPtr);
  }
}

void Initializer::fillStruct(codegen::ModuleContext& ctx, std::shared_ptr<meta::Type>& t, llvm::AllocaInst * alloca, PayloadList payload) {
  auto * structTy = llvm::cast<llvm::StructType>(t->getLLVMType(ctx));;

  // Zero-initialize the whole thing to have a determined state for uninitialized fields
  ctx.ir_builder->CreateStore(llvm::ConstantAggregateZero::get(structTy), alloca);

  for (auto& value : values) {
    /* Allow for struct initializers to skip field name, if it is initialized with a variable of the same name. E.g:
     * ```
     * struct Test {
     *   x: i32;
     *   y: i32;
     * }
     *
     * var x = 10;
     * var y = 10;
     *
     * var test = Test { x, y };
     * ```
     *
     * The last line will (thanks to the next 3 lines) be parsed as:
     * ```
     * var test = Test { x: x, y: y };
     * ```
     *
     * This is just to remove boilerplate
     */
    if (!value.name && value.value->is(AST_EXPR_IDENTIFIER) && t->hasMember(value.value->as<Identifier>()->value)) {
      value.name = value.value;
    }

    // Struct initializers must name every field
    assertRaise(value.name && value.name->is(AST_EXPR_IDENTIFIER),
      Error(ERROR_INIT_EXPECTED_NAMED_VALUE, value.name ? value.name->span : value.value->span));

    uint32_t fieldIdx = t->getMemberIndex(value.name->as<Identifier>()->value);

    auto * fieldPtr = ctx.ir_builder->CreateStructGEP(structTy, alloca, fieldIdx);

    auto * val = value.value->generateValue(ctx, payload);

    ctx.ir_builder->CreateStore(val, fieldPtr);
  }
}

void Initializer::fillArray(codegen::ModuleContext& ctx, std::shared_ptr<meta::Type>& t, llvm::AllocaInst * alloca, PayloadList payload) {
  auto * arrayTy = t->getLLVMArrayType(ctx);

  // Hint for nested initializers
  payload = extendPayload(excludePayload(payload, AST_INIT), Initializer::Payload::create(t->getElementType()));

  size_t i = 0;

  for (; i < values.size(); ++i) {
    // Offset from the base pointer + Index of the child element
    std::vector<llvm::Value *> indices = {ctx.ir_builder->getInt32(0), ctx.ir_builder->getInt32(i)};
    auto * elementPtr = ctx.ir_builder->CreateInBoundsGEP(arrayTy, alloca, indices);
    auto * val = castIfNotSame(ctx, values[i].value->generateValue(ctx, payload), arrayTy->getElementType(), values[i].value->span);
    ctx.ir_builder->CreateStore(val, elementPtr);
  }

  // Fill the rest with zeros
  // TODO: Can llvm::ConstantAggregateZero be used for partial initialization?
  if (i != t->getElementCount()) {
    for (size_t j = i; j < t->getElementCount(); ++j) {
      std::vector<llvm::Value *> indices = {ctx.ir_builder->getInt32(0), ctx.ir_builder->getInt32(j)};
      auto * elementPtr = ctx.ir_builder->CreateInBoundsGEP(arrayTy, alloca, indices);
      ctx.ir_builder->CreateStore(t->getElementType()->getDefault(ctx), elementPtr);
    }
  }
}

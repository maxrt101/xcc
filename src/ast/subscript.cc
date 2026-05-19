#include "xcc/ast/subscript.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;

Subscript::Subscript(SourceSpan span, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
  : Node(AST_EXPR_SUBSCRIPT, span), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

std::shared_ptr<Subscript> Subscript::create(SourceSpan span, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs) {
  return std::make_shared<Subscript>(span, std::move(lhs), std::move(rhs));
}

std::shared_ptr<Node> Subscript::clone() {
  return withAttrs(create(span, lhs->clone(), rhs->clone()));
}

void Subscript::visit(std::unique_ptr<codegen::GlobalContext>& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(globalContext, lhs, visitor, ignoreSubtree);
  callVisitor(globalContext, rhs, visitor, ignoreSubtree);
}

std::string Subscript::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(0, false) + std::format("{}[{}]",
    lhs->toString(parent, this, indent, false),
    rhs->toString(parent, this, indent, false)
  );
}

llvm::Value * Subscript::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  auto base_type   = raiseIfNull(lhs->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, lhs->span, "LHS Type is NULL"));
  auto element_ptr = generateValueWithoutLoad(ctx, payload);

  if (base_type->isTuple()) {
    return ctx.ir_builder->CreateLoad(generateType(ctx, payload)->getLLVMType(ctx), element_ptr, "tuple_element");
  }

  return ctx.ir_builder->CreateLoad(base_type->getBaseType()->getLLVMType(ctx), element_ptr, "element");
}

llvm::Value * Subscript::generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  auto base_type  = raiseIfNull(lhs->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, lhs->span, "LHS Type is NULL"));
  auto index_type = raiseIfNull(rhs->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, rhs->span, "RHS Type is NULL"));

  assertRaiseFromNode(base_type->isPointer() || base_type->isArray() || base_type->isTuple(),
    Error(ERROR_TYPE_NOT_SUBSCRIPTABLE, lhs->span, "'{}'", base_type->toString()), this);

  assertRaiseFromNode(index_type->isInteger(),
    Error(ERROR_TYPE_NOT_VALID_SUBSCRIPT, rhs->span, "'{}'", index_type->toString()), this);

  auto base_ptr_val = raiseIfNull(lhs->generateValueWithoutLoad(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, lhs->span, "LHS Value is NULL"));

  if (base_type->isTuple()) {
    auto * const_idx = llvm::cast<llvm::ConstantInt>(rhs->generateConstant(ctx, payload));

    assertRaiseFromNode(const_idx, Error(ERROR_NOT_CONSTANT, rhs->span, "Tuple subscript index must be a constant"), this);

    return ctx.ir_builder->CreateStructGEP(
        base_type->getLLVMType(ctx),
        base_ptr_val,
        const_idx->getZExtValue(),
        "tuple_member"
    );
  }

  auto index             = raiseIfNull(rhs->generateValue(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, rhs->span, "RHS Value is NULL"));
  auto element_type_llvm = base_type->getBaseType()->getLLVMType(ctx);

  if (base_type->isPointer()) {
    // For pointers, we must load the address stored in the alloca first
    // base_ptr_val is T**, we want T*
    auto * actual_address = ctx.ir_builder->CreateLoad(base_type->getLLVMType(ctx), base_ptr_val, "ptr_load");

    // GEP for pointers usually takes a single index
    return ctx.ir_builder->CreateInBoundsGEP(element_type_llvm, actual_address, index, "element_ptr");
  }

  // For arrays, the alloca is the base address
  // We need the 0-index to step into the array type
  std::vector<llvm::Value*> indices = {
    ctx.ir_builder->getInt32(0),
    index
  };

  return ctx.ir_builder->CreateInBoundsGEP(base_type->getLLVMType(ctx), base_ptr_val, indices, "element_ptr");
}

std::shared_ptr<xcc::meta::Type> Subscript::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  auto base_type = raiseIfNull(lhs->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, lhs->span, "LHS Type is NULL"));

  if (base_type->isTuple()) {
    auto * const_idx = llvm::cast<llvm::ConstantInt>(rhs->generateConstant(ctx, payload));
    assertRaiseFromNode(const_idx, Error(ERROR_NOT_CONSTANT, rhs->span, "Tuple subscript index must be a constant"), this);

    return base_type->getTupleMemberType(const_idx->getZExtValue());
  }

  return base_type->getBaseType();
}

std::shared_ptr<xcc::meta::Type> Subscript::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  return generateType(ctx, payload);
}

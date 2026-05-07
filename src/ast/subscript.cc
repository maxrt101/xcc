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

void Subscript::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(lhs, visitor, ignoreSubtree);
  callVisitor(rhs, visitor, ignoreSubtree);
}

std::string Subscript::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(0, false) + std::format("{}[{}]",
    lhs->toString(parent, this, indent, false),
    rhs->toString(parent, this, indent, false)
  );
}

llvm::Value * Subscript::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  auto base_type = raiseIfNull(lhs->generateType(ctx, {}), Error(ERROR_INTERNAL_UNEXPECTED_NULL, lhs->span, "LHS Type is NULL"));
  auto element_ptr = generateValueWithoutLoad(ctx, payload);

  return ctx.ir_builder->CreateLoad(base_type->getBaseType()->getLLVMType(ctx), element_ptr, "element");
}

llvm::Value * Subscript::generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  auto base_type = raiseIfNull(lhs->generateType(ctx, {}), Error(ERROR_INTERNAL_UNEXPECTED_NULL, lhs->span, "LHS Type is NULL"));
  auto index_type = raiseIfNull(rhs->generateType(ctx, {}), Error(ERROR_INTERNAL_UNEXPECTED_NULL, rhs->span, "RHS Type is NULL"));

  assertRaiseFromNode(base_type->isPointer() || base_type->isArray(), Error(ERROR_TYPE_NOT_SUBSCRIPTABLE, lhs->span, "'{}'", base_type->toString()), this);
  assertRaiseFromNode(index_type->isInteger(), Error(ERROR_TYPE_NOT_VALID_SUBSCRIPT, rhs->span, "'{}'", index_type->toString()), this);

  auto base_ptr = raiseIfNull(lhs->generateValue(ctx, {}), Error(ERROR_INTERNAL_UNEXPECTED_NULL, lhs->span, "LHS Value is NULL"));
  auto index = raiseIfNull(rhs->generateValue(ctx, {}), Error(ERROR_INTERNAL_UNEXPECTED_NULL, rhs->span,"RHS Value is NULL"));

  return ctx.ir_builder->CreateGEP(base_type->getBaseType()->getLLVMType(ctx), base_ptr, index, "element_ptr");
}

std::shared_ptr<xcc::meta::Type> Subscript::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  auto base_type = raiseIfNull(lhs->generateType(ctx, {}), Error(ERROR_INTERNAL_UNEXPECTED_NULL, lhs->span, "LHS Type is NULL"));
  return base_type->getBaseType();
}

std::shared_ptr<xcc::meta::Type> Subscript::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  return generateType(ctx, payload);
}

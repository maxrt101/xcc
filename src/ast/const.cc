#include "xcc/ast/const.h"
#include "xcc/ast/number.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;

ConstDecl::ConstDecl(
  SourceSpan                  span,
  std::shared_ptr<Identifier> name,
  std::shared_ptr<Node>       type,
  std::shared_ptr<Node>       value
) : Node(AST_CONST_DECL, span),
    name(std::move(name)),
    type(std::move(type)),
    value(std::move(value)) {}

std::shared_ptr<ConstDecl> ConstDecl::create(
  SourceSpan                  span,
  std::shared_ptr<Identifier> name,
  std::shared_ptr<Node>       type,
  std::shared_ptr<Node>       value
) {
  return std::make_shared<ConstDecl>(span, std::move(name), std::move(type), std::move(value));
}

std::shared_ptr<Node> ConstDecl::clone() {
  return withAttrs(create(span,
    cast<Identifier>(name->clone()),
    type ? type->clone() : nullptr,
    value ? value->clone() : nullptr
  ));
}

void ConstDecl::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(globalContext, name, visitor, ignoreSubtree);
  callVisitor(globalContext, type, visitor, ignoreSubtree);
  callVisitor(globalContext, value, visitor, ignoreSubtree);
}

std::string ConstDecl::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(indent, newline) + "const " + name->toString(parent, this, indent, false);

  if (type) {
    res += ": " + type->toString(parent, this, indent, false);
  }

  if (value) {
    res += " = " + value->toString(parent, this, indent, false);
  }

  return res;
}

llvm::Constant * ConstDecl::generateConstant(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  // If both type and value are missing - fail, if one is present - the other can be (usually) inferred
  assertRaiseFromNode(type || value, Error(ERROR_VARDECL_NO_VAL_AND_TYPE, span), this);

  if (type) {
    assertRaiseFromNode(isOrIsLastInBlock(type, AST_EXPR_TYPE),
      Error(ERROR_NOT_A_TYPE, type->span, "got a {}", typeToHumanReadableString(getOrGetLastInBlock(type)->type)), this);

    payload = extendPayload(payload, Initializer::Payload::create(type->generateType(ctx, payload)));
  }

  auto meta_type = type ? type->generateType(ctx, payload) : meta::Type::inferFromNode(ctx, value);

  if (meta_type->isInteger()) {
    payload = extendPayload(payload, Number::Payload::create(meta_type->getNumberBitWidth()));
  }

  return value
      ? value->generateConstant(ctx, payload)
      : meta_type->getDefault(ctx);
}

std::shared_ptr<xcc::meta::Type> ConstDecl::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  // If both type and value are missing - fail, if one is present - the other can be (usually) inferred
  assertRaiseFromNode(type || value, Error(ERROR_VARDECL_NO_VAL_AND_TYPE, span), this);

  return type ? type->generateType(ctx, payload) : meta::Type::inferFromNode(ctx, value);
}


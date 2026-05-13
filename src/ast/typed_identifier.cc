#include "xcc/ast/typed_identifier.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

TypedIdentifier::TypedIdentifier(SourceSpan span, std::shared_ptr<Identifier> name, std::shared_ptr<Node> type, std::shared_ptr<Node> value)
  : Node(AST_EXPR_TYPED_IDENTIFIER, span), name(std::move(name)), value_type(std::move(type)), value(std::move(value)) {}

std::shared_ptr<TypedIdentifier> TypedIdentifier::create(SourceSpan span, std::shared_ptr<Identifier> name, std::shared_ptr<Node> type, std::shared_ptr<Node> value) {
  return std::make_shared<TypedIdentifier>(span, std::move(name), std::move(type), std::move(value));
}

std::shared_ptr<Node> TypedIdentifier::clone() {
  return withAttrs(create(span, cast<Identifier>(
    name->clone()),
    value_type ? cast<Type>(value_type->clone()) : nullptr,
    value ? value->clone() : nullptr
  ));
}

void TypedIdentifier::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(name, visitor, ignoreSubtree);
  callVisitor(value_type, visitor, ignoreSubtree);
  callVisitor(value, visitor, ignoreSubtree);
}

std::string TypedIdentifier::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(0, false) + name->toString(parent, this, indent, false);

  if (value_type) {
    res += ": " + value_type->toString(parent, this, indent, false);
  }

  if (value) {
    res += " = " + value->toString(parent, this, indent, false);
  }

  return res;
}

std::shared_ptr<xcc::meta::Type> TypedIdentifier::generateType(codegen::ModuleContext &ctx, PayloadList payload) {
  auto t = value_type ? value_type : value;

  if (!t) {
    Error(ERROR_MISSING_TYPE, span, "Can't infer value's type, because no type or default value is present").raiseFromNode(this);
  }

  return t->generateType(ctx, payload);
}

#include "xcc/ast/typed_identifier.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

TypedIdentifier::TypedIdentifier(std::shared_ptr<Identifier> name, std::shared_ptr<Type> type, std::shared_ptr<Node> value)
  : Node(AST_EXPR_TYPED_IDENTIFIER), name(std::move(name)), value_type(std::move(type)), value(std::move(value)) {}

std::shared_ptr<TypedIdentifier> TypedIdentifier::create(std::shared_ptr<Identifier> name, std::shared_ptr<Type> type, std::shared_ptr<Node> value) {
  return std::make_shared<TypedIdentifier>(std::move(name), std::move(type), std::move(value));
}

std::shared_ptr<xcc::meta::Type> TypedIdentifier::generateType(codegen::ModuleContext &ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  return value_type->generateType(ctx, {});
}

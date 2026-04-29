#include "xcc/ast/typedecl.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc;
using namespace xcc::ast;

TypeDecl::TypeDecl(std::shared_ptr<Node> name, std::shared_ptr<Node> value)
  : Node(AST_TYPE_DECL), name(std::move(name)), value(std::move(value)) {}

std::shared_ptr<TypeDecl> TypeDecl::create(std::shared_ptr<Node> name, std::shared_ptr<Node> value) {
  return std::make_shared<TypeDecl>(std::move(name), std::move(value));
}

std::shared_ptr<Node> TypeDecl::clone() {
  return withAttrs(create(name->clone(), value->clone()));
}

void TypeDecl::visit(Visitor visitor) {
  callVisitor(name, visitor);
  callVisitor(value, visitor);
}

std::shared_ptr<meta::Type> TypeDecl::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  assertThrow(name->is(AST_EXPR_IDENTIFIER), CodegenException("Type alias (declaration) name must be an identifier"));
  assertThrow(value->is(AST_EXPR_TYPE), CodegenException("Type alias (declaration) value must be a type expr"));

  auto alias = name->as<Identifier>()->name();

  if (!meta::Type::hasCustomType(alias)) {
    meta::Type::registerCustomType(alias, value->as<Type>()->generateType(ctx, payload));
  }

  return meta::Type::fromTypeName(ctx.globalContext, alias);
}

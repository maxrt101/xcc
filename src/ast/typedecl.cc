#include "xcc/ast/typedecl.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc;
using namespace xcc::ast;

TypeDecl::TypeDecl(SourceSpan span, std::shared_ptr<Node> name, std::shared_ptr<Node> value)
  : Node(AST_TYPE_DECL, span), name(std::move(name)), value(std::move(value)) {}

std::shared_ptr<TypeDecl> TypeDecl::create(SourceSpan span, std::shared_ptr<Node> name, std::shared_ptr<Node> value) {
  return std::make_shared<TypeDecl>(span, std::move(name), std::move(value));
}

std::shared_ptr<Node> TypeDecl::clone() {
  return withAttrs(create(span, name->clone(), value->clone()));
}

void TypeDecl::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(name, visitor, ignoreSubtree);
  callVisitor(value, visitor, ignoreSubtree);
}

std::string TypeDecl::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(indent, newline) + std::format("type {} = {}",
    name->toString(parent, this, indent, false),
    value->toString(parent, this, indent, false)
  );
}

std::shared_ptr<meta::Type> TypeDecl::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  assertRaiseFromNode(name->is(AST_EXPR_IDENTIFIER), Error(ERROR_TYPE_ALIAS_NAME_NOT_IDENTIFIER, name->span, ""), this);
  assertRaiseFromNode(value->is(AST_EXPR_TYPE), Error(ERROR_TYPE_ALIAS_VALUE_NOT_TYPE, value->span, ""), this);

  auto alias = name->as<Identifier>()->name();

  if (!meta::Type::hasCustomType(alias)) {
    meta::Type::registerCustomType(alias, value->as<Type>()->generateType(ctx, payload));
  }

  return meta::Type::fromTypeName(ctx.globalContext, alias, name->span);
}

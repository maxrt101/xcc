#include "xcc/ast/type.h"
#include "xcc/ast/identifier.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/meta/type.h"

using namespace xcc::ast;

Type::Type(std::shared_ptr<Node> name, bool pointer) : Node(AST_EXPR_TYPE), name(std::move(name)), pointer(pointer) {}

std::shared_ptr<Type> Type::create(std::shared_ptr<Node> name, bool pointer) {
  return std::make_shared<Type>(std::move(name), pointer);
}

std::shared_ptr<xcc::meta::Type> Type::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  /* Basic type - identifier + optional pointer */
  if (name->is(AST_EXPR_IDENTIFIER)) {
    auto baseType = meta::Type::fromTypeName(name->as<Identifier>()->value);
    return pointer ? meta::Type::createPointer(baseType) : baseType;
  }

  /* Recursive type - type + optional pointer */
  if (name->is(AST_EXPR_TYPE)) {
    auto baseType = name->as<Type>()->generateType(ctx, payload);
    return pointer ? meta::Type::createPointer(baseType) : baseType;
  }

  throw CodegenException("Unexpected type node '" + Node::typeToString(name->type) + "' (" + std::to_string(name->type) +")");
}

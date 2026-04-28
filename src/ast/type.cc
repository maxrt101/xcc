#include "xcc/ast/type.h"
#include "xcc/ast/identifier.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/meta/type.h"

using namespace xcc::ast;

Type::Type(std::shared_ptr<Node> name, bool pointer)
  : Node(AST_EXPR_TYPE), name(std::move(name)), pointer(pointer), function(false) {}

Type::Type(std::shared_ptr<Type> returnType, std::vector<std::shared_ptr<Type>> args, bool isVariadic)
  : Node(AST_EXPR_TYPE), function(true), isVariadic(isVariadic), returnType(returnType), args(args) {}

std::shared_ptr<Type> Type::create(std::shared_ptr<Node> name, bool pointer) {
  return std::make_shared<Type>(std::move(name), pointer);
}

std::shared_ptr<Type> Type::createFunction(std::shared_ptr<Type> returnType, std::vector<std::shared_ptr<Type>> args, bool isVariadic) {
  return std::make_shared<Type>(std::move(returnType), std::move(args), isVariadic);
}

std::shared_ptr<xcc::meta::Type> Type::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  if (function) {
    std::vector<std::shared_ptr<meta::Type>> args;

    for (auto& arg : this->args) {
      args.push_back(arg->generateType(ctx, payload));
    }

    return meta::Type::createFunction(returnType->generateType(ctx, payload), args, isVariadic);
  }

  /* Basic type - identifier + optional pointer */
  if (name->is(AST_EXPR_IDENTIFIER)) {
    auto baseType = meta::Type::fromTypeName(name->as<Identifier>()->name());
    return pointer ? meta::Type::createPointer(baseType) : baseType;
  }

  /* Recursive type - type + optional pointer */
  if (name->is(AST_EXPR_TYPE)) {
    auto baseType = name->as<Type>()->generateType(ctx, payload);
    return pointer ? meta::Type::createPointer(baseType) : baseType;
  }

  throw CodegenException("Unexpected type node '" + Node::typeToString(name->type) + "' (" + std::to_string(name->type) +")");
}

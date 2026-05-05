#include "xcc/ast/type.h"
#include "xcc/ast/identifier.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/meta/type.h"

using namespace xcc::ast;

Type::Type(SourceSpan span, std::shared_ptr<Node> name, bool pointer)
  : Node(AST_EXPR_TYPE, span), name(std::move(name)), pointer(pointer), function(false) {}

Type::Type(SourceSpan span, std::shared_ptr<Node> returnType, std::vector<std::shared_ptr<Node>> args, bool isVariadic)
  : Node(AST_EXPR_TYPE, span), function(true), isVariadic(isVariadic), returnType(returnType), args(args) {}

std::shared_ptr<Type> Type::create(SourceSpan span, std::shared_ptr<Node> name, bool pointer) {
  return std::make_shared<Type>(span, std::move(name), pointer);
}

std::shared_ptr<Type> Type::createFunction(SourceSpan span, std::shared_ptr<Node> returnType, std::vector<std::shared_ptr<Node>> args, bool isVariadic) {
  return std::make_shared<Type>(span, std::move(returnType), std::move(args), isVariadic);
}

std::shared_ptr<Node> Type::clone() {
  if (function) {
    return withAttrs(createFunction(span, cast<Type>(returnType->clone()), cloneVector(args), isVariadic));
  }

  return withAttrs(create(span, name->clone(), pointer));
}

void Type::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  if (function) {
    callVisitor(returnType, visitor, ignoreSubtree);

    for (auto& node : args) {
      callVisitor(node, visitor, ignoreSubtree);
    }
  } else {
    callVisitor(name, visitor, ignoreSubtree);
  }
}

std::string Type::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(0, false);

  if (function) {
    res = "fn (";

    for (size_t i = 0; i < args.size(); ++i) {
      res += args[i]->toString(parent, this, indent, false);

      if (i + 1 < args.size()) {
        res += ", ";
      }
    }

    res += ") -> " + returnType->toString(parent, this, indent, false);
  } else {
    res = name->toString(parent, this, indent, false);
  }

  return res;
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
    auto baseType = meta::Type::fromTypeName(ctx.globalContext, name->as<Identifier>()->name());
    return pointer ? meta::Type::createPointer(baseType) : baseType;
  }

  /* Recursive type - type + optional pointer */
  if (name->is(AST_EXPR_TYPE)) {
    auto baseType = name->as<Type>()->generateType(ctx, payload);
    return pointer ? meta::Type::createPointer(baseType) : baseType;
  }

  throw Error(ERROR_INTERNAL_UNEXPECTED_NODE, name->span, "Unexpected type node '" + Node::typeToString(name->type) + "' (" + std::to_string(name->type) +")");
}

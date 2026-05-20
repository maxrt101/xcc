#include "xcc/ast/type.h"
#include "xcc/ast/identifier.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/meta/type.h"

using namespace xcc::ast;

Type::Type(SourceSpan span, Kind kind, std::shared_ptr<Node> name)
  : Node(AST_EXPR_TYPE, span), kind(kind), name(std::move(name)) {}

std::shared_ptr<Type> Type::create(SourceSpan span, std::shared_ptr<Node> name) {
  return std::make_shared<Type>(span, NORMAL, std::move(name));
}

std::shared_ptr<Type> Type::createPointer(SourceSpan span, std::shared_ptr<Node> name) {
  return std::make_shared<Type>(span, POINTER, std::move(name));
}

std::shared_ptr<Type> Type::createArray(SourceSpan span, std::shared_ptr<Node> name, std::shared_ptr<Node> size) {
  auto t = std::make_shared<Type>(span, ARRAY, nullptr);
  t->name       = std::move(name);
  t->array.size = std::move(size);
  return t;
}

std::shared_ptr<Type> Type::createFunction(SourceSpan span, std::shared_ptr<Node> returnType, NodeList args, bool isVariadic) {
  auto t = std::make_shared<Type>(span, FUNCTION, nullptr);
  t->fn.returnType = std::move(returnType);
  t->fn.args       = std::move(args);
  t->fn.isVariadic = isVariadic;
  return t;
}

std::shared_ptr<Type> Type::createLambda(SourceSpan span, NodeList captures, std::shared_ptr<Node> returnType, NodeList args, bool isVariadic) {
  auto t = std::make_shared<Type>(span, LAMBDA, nullptr);
  t->lambda.captures = std::move(captures);
  t->fn.returnType   = std::move(returnType);
  t->fn.args         = std::move(args);
  t->fn.isVariadic   = isVariadic;
  return t;
}

std::shared_ptr<Type> Type::createTuple(SourceSpan span, NodeList members) {
  auto t = std::make_shared<Type>(span, TUPLE, nullptr);
  t->tuple.members = std::move(members);
  return t;
}

std::shared_ptr<Node> Type::clone() {
  switch (kind) {
    case POINTER:
      return withAttrs(createPointer(span, name->clone()));
    case ARRAY:
      return withAttrs(createArray(span, name->clone(), array.size->clone()));
    case FUNCTION:
      return withAttrs(createFunction(span, cast<Type>(fn.returnType->clone()), cloneVector(fn.args), fn.isVariadic));
    case LAMBDA:
      return withAttrs(createLambda(span, cloneVector(lambda.captures), cast<Type>(fn.returnType->clone()), cloneVector(fn.args), fn.isVariadic));
    case TUPLE:
      return withAttrs(createTuple(span, cloneVector(tuple.members)));
    case NORMAL:
    default:
      return withAttrs(create(span, name->clone()));
  }
}

void Type::visit(std::unique_ptr<codegen::GlobalContext>& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  switch (kind) {
    case LAMBDA:
      for (auto& node : lambda.captures) {
        callVisitor(globalContext, node, visitor, ignoreSubtree);
      }
      [[fallthrough]];
    case FUNCTION:
      callVisitor(globalContext, fn.returnType, visitor, ignoreSubtree);
      for (auto& node : fn.args) {
        callVisitor(globalContext, node, visitor, ignoreSubtree);
      }
      break;
    case ARRAY:
      callVisitor(globalContext, array.size, visitor, ignoreSubtree);
      [[fallthrough]];
    case NORMAL:
    case POINTER:
    default:
      callVisitor(globalContext, name, visitor, ignoreSubtree);
      break;
  }
}

std::string Type::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(0, false);

  if (kind == TUPLE) {
    res = "[";

    for (size_t i = 0; i < tuple.members.size(); ++i) {
      res += tuple.members[i]->toString(parent, this, indent, false);

      if (i + 1 < tuple.members.size()) {
        res += ", ";
      }
    }

    return res + "]";
  }

  if (kind == FUNCTION || kind == LAMBDA) {
    res = "fn ";

    if (kind == LAMBDA) {
      res += "[";
      for (size_t i = 0; i < lambda.captures.size(); ++i) {
        res += lambda.captures[i]->toString(parent, this, indent, false);
        if (i + 1 < lambda.captures.size()) {
          res += ", ";
        }
      }
      res += "] ";
    }

    res += "(";

    for (size_t i = 0; i < fn.args.size(); ++i) {
      res += fn.args[i]->toString(parent, this, indent, false);

      if (i + 1 < fn.args.size()) {
        res += ", ";
      }
    }

    return res + ") -> " + fn.returnType->toString(parent, this, indent, false);
  }

  res += name->toString(parent, this, indent, false);

  if (kind == ARRAY) {
    res += std::format("[{}]", array.size->toString(parent, this, indent, false));
  }

  if (kind == POINTER) {
    res += "*";
  }

  return res;
}

std::shared_ptr<xcc::meta::Type> Type::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  if (kind == TUPLE) {
    std::vector<std::shared_ptr<meta::Type>> members;

    for (auto& arg : this->tuple.members) {
      members.push_back(arg->generateType(ctx, payload));
    }

    return meta::Type::createTuple(members);
  }

  if (kind == FUNCTION) {
    std::vector<std::shared_ptr<meta::Type>> args;

    for (auto& arg : this->fn.args) {
      args.push_back(arg->generateType(ctx, payload));
    }

    return meta::Type::createFunction(fn.returnType->generateType(ctx, payload), args, fn.isVariadic);
  }

  if (kind == LAMBDA) {
    meta::StructMembers captures;

    for (size_t i = 0; i < lambda.captures.size(); ++i) {
      captures.emplace_back(std::to_string(i), lambda.captures[i]->generateType(ctx, payload));
    }

    std::vector<std::shared_ptr<meta::Type>> args;

    for (auto& arg : this->fn.args) {
      args.push_back(arg->generateType(ctx, payload));
    }

    return meta::Type::createLambda(
      meta::Type::createFunction(fn.returnType->generateType(ctx, payload), args, fn.isVariadic),
      captures
    );
  }

  auto baseType = getBaseType(ctx, payload);

  if (kind == ARRAY) {
    auto n = array.size->generateConstant(ctx, payload);

    if (auto s = llvm::dyn_cast<llvm::ConstantInt>(n)) {
      return meta::Type::createArray(baseType, s->getValue().getZExtValue());
    }

    Error(ERROR_TYPE_ARRAY_SIZE_NOT_NUMBER, array.size->span,
      "'{}' does not evaluate to a constant integer",
      array.size->toString(nullptr, nullptr, 0, false)
    ).raiseFromNode(this);
  }

  return kind == POINTER ? meta::Type::createPointer(baseType) : baseType;
}

std::shared_ptr<xcc::meta::Type> Type::getBaseType(codegen::ModuleContext& ctx, PayloadList payload) {
  if (name->is(AST_EXPR_IDENTIFIER)) {
    return meta::Type::fromTypeName(
      ctx.globalContext,
      ctx.globalContext.aliased(name->as<Identifier>()->name()),
      name->span
    );
  }

  /* Recursive type - type + optional pointer */
  if (name->is(AST_EXPR_TYPE)) {
    return name->as<Type>()->generateType(ctx, payload);
  }

  Error(ERROR_INTERNAL_UNEXPECTED_NODE, name->span, "Unexpected {} at type", typeToHumanReadableString(name->type)).raiseFromNode(this);
}

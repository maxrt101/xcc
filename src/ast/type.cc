#include "xcc/ast/type.h"
#include "xcc/ast/identifier.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/meta/type.h"

using namespace xcc::ast;

Type::Payload::Payload(meta::SubstitutionMap sub)
  : Node::Payload(AST_EXPR_TYPE), substitutions(std::move(sub)) {}

std::shared_ptr<Node::Payload> Type::Payload::create(meta::SubstitutionMap sub) {
  return std::dynamic_pointer_cast<Node::Payload>(
      std::make_shared<Type::Payload>(std::move(sub))
  );
}

Type::Type(SourceSpan span, Kind kind, std::shared_ptr<Node> name)
  : Node(AST_EXPR_TYPE, span), kind(kind), name(std::move(name)) {}

std::shared_ptr<Type> Type::create(SourceSpan span, std::shared_ptr<Node> name) {
  return std::make_shared<Type>(span, NORMAL, std::move(name));
}

std::shared_ptr<Type> Type::createGeneric(SourceSpan span, std::shared_ptr<Node> name, NodeList genericArgs) {
  auto t = std::make_shared<Type>(span, NORMAL, std::move(name));
  t->genericArgs = std::move(genericArgs);
  return t;
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

bool Type::isGeneric() const {
  return !genericArgs.empty();
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
      return withAttrs(isGeneric() ? createGeneric(span, name->clone(), cloneVector(genericArgs)) : create(span, name->clone()));
  }
}

void Type::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
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
    default: {
      callVisitor(globalContext, name, visitor, ignoreSubtree);
      if (isGeneric()) {
        for (auto& node : genericArgs) {
          callVisitor(globalContext, node, visitor, ignoreSubtree);
        }
      }
      break;
    }
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

  if (isGeneric()) {
    res += "<";
    for (size_t i = 0; i < genericArgs.size(); ++i) {
      res += genericArgs[i]->toString(parent, this, indent, false);
      if (i + 1 < genericArgs.size()) {
        res += ", ";
      }
    }
    res += ">";
  }

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
    std::vector<std::shared_ptr<meta::Type>> captures, args;

    for (size_t i = 0; i < lambda.captures.size(); ++i) {
      captures.emplace_back(lambda.captures[i]->generateType(ctx, payload));
    }

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
    auto id = name->as<Identifier>()->name();

    if (isGeneric() && codegen::GenericsCache::has(id)) {
      auto generic = codegen::GenericsCache::get(id);

      auto original_id = id;

      // Create a new instantiated/mangled type name ('mod::Struct<T>' -> 'mod_Struct_i32')
      id = name->as<Identifier>()->fullName(ctx, payload, false, genericArgs);

      // Don't generate if already generated
      if (meta::Type::hasCustomType(id)) {
        return meta::Type::getCustomType(id);
      }

      // Create and register new custom type, needed at this stage for correct macro expansion
      // as method may contain a macro call, which may need to know concrete type for generic struct
      auto type = meta::Type::createStruct(id, {}, generic->hasAttribute("packed"));
      meta::Type::registerCustomType(id, type);

      assertRaiseFromNode(generic->is(AST_STRUCT), Error(ERROR_UNIMPLEMENTED, generic->span, "Only generic structs are supported"), this);

      // Clone generic struct declaration. This is needed, as FnDecl will
      // modify itself, if payload with concrete struct name is passed to it
      generic = generic->clone();

      std::vector<std::string> genericTypes;

      // Extract generic type names
      for (auto& g : generic->as<Struct>()->genericTypes) {
        assertRaiseFromNode(g->is(AST_EXPR_IDENTIFIER), Error(ERROR_UNIMPLEMENTED, g->span, "Only an identifier can be a generic type"), this);
        genericTypes.push_back(g->as<Identifier>()->name());
      }

      // TODO: Don't forget to change this assert, when default generic types are implemented
      assertRaiseFromNode(genericTypes.size() == genericArgs.size(), Error(ERROR_GENERIC_COUNT_MISMATCH, span), this);

      // Initialize substitution map (generic arg -> meta type) with generalized
      // name -> concrete type ('Container' -> 'meta::Type("Container_i32" for T=i32)')
      meta::SubstitutionMap substitutions = {{original_id, create(name->span, Identifier::create(name->span, id))->generateType(ctx, payload)}};

      // Generate substitures for each generic type
      for (size_t i = 0; i < genericTypes.size(); ++i) {
        substitutions[genericTypes[i]] = genericArgs[i]->generateType(ctx, payload);
      }

      // Create Type::Payload with substitutions, will be
      // referred to each time a generic type is requested
      payload.push_back(Type::Payload::create(substitutions));

      // Create a Struct::Payload, with new concrete name
      payload.push_back(Struct::Payload::create(id));

      auto struct_type = generic->generateType(ctx, payload);

      llvm::IRBuilder<>::InsertPointGuard ir_guard(*ctx.ir_builder);

      for (auto& method : generic->as<Struct>()->methods) {
        // Sequentially generate all methods, adding FnDecl::Payload
        // with generic struct name -> concrete name substitution
        // TODO: This could be added once along with other payloads
        method->generateFunction(ctx, extendPayload(payload, FnDecl::Payload::create(original_id, id)));
      }

      return struct_type;
    }

    if (auto p = selectPayloadFirst(payload)) {
      auto sub = p->as<Type::Payload>();

      // Check substitution map, if payload is present
      if (sub->substitutions.contains(id)) {
        return sub->substitutions[id];
      }
    }

    return meta::Type::fromTypeName(
      ctx.globalContext,
      ctx.globalContext.aliased(id),
      name->span
    );
  }

  /* Recursive type - type + optional pointer */
  if (name->is(AST_EXPR_TYPE)) {
    return name->as<Type>()->generateType(ctx, payload);
  }

  Error(ERROR_INTERNAL_UNEXPECTED_NODE, name->span, "Unexpected {} at type", typeToHumanReadableString(name->type)).raiseFromNode(this);
}

#include "xcc/ast/type.h"
#include "xcc/ast/identifier.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/meta/type.h"
#include "xcc/util/log.h"

using namespace xcc::ast;

static auto& logger = xcc::log::Logger::get("GENERICS");

Type::Type(SourceSpan span, LexicalScope scope, Kind kind, std::shared_ptr<Node> name)
  : Node(AST_EXPR_TYPE, span, scope), kind(kind), name(std::move(name)) {}

std::shared_ptr<Type> Type::create(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> name) {
  return std::make_shared<Type>(span, scope, NORMAL, std::move(name));
}

std::shared_ptr<Type> Type::createPointer(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> name) {
  return std::make_shared<Type>(span, scope, POINTER, std::move(name));
}

std::shared_ptr<Type> Type::createArray(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> name, std::shared_ptr<Node> size) {
  auto t = std::make_shared<Type>(span, scope, ARRAY, nullptr);
  t->name       = std::move(name);
  t->array.size = std::move(size);
  return t;
}

std::shared_ptr<Type> Type::createFunction(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> returnType, NodeList args, bool isVariadic) {
  auto t = std::make_shared<Type>(span, scope, FUNCTION, nullptr);
  t->fn.returnType = std::move(returnType);
  t->fn.args       = std::move(args);
  t->fn.isVariadic = isVariadic;
  return t;
}

std::shared_ptr<Type> Type::createLambda(SourceSpan span, LexicalScope scope, NodeList captures, std::shared_ptr<Node> returnType, NodeList args, bool isVariadic) {
  auto t = std::make_shared<Type>(span, scope, LAMBDA, nullptr);
  t->lambda.captures = std::move(captures);
  t->fn.returnType   = std::move(returnType);
  t->fn.args         = std::move(args);
  t->fn.isVariadic   = isVariadic;
  return t;
}

std::shared_ptr<Type> Type::createTuple(SourceSpan span, LexicalScope scope, NodeList members) {
  auto t = std::make_shared<Type>(span, scope, TUPLE, nullptr);
  t->tuple.members = std::move(members);
  return t;
}

std::shared_ptr<Node> Type::clone() {
  switch (kind) {
    case POINTER:
      return withAttrs(createPointer(span, scope, name->clone()));
    case ARRAY:
      return withAttrs(createArray(span, scope, name->clone(), array.size->clone()));
    case FUNCTION:
      return withAttrs(createFunction(span, scope, cast<Type>(fn.returnType->clone()), cloneVector(fn.args), fn.isVariadic));
    case LAMBDA:
      return withAttrs(createLambda(span, scope, cloneVector(lambda.captures), cast<Type>(fn.returnType->clone()), cloneVector(fn.args), fn.isVariadic));
    case TUPLE:
      return withAttrs(createTuple(span, scope, cloneVector(tuple.members)));
    case NORMAL:
    default:
      return withAttrs(create(span, scope, name->clone()));
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
  if (!name->isAnyOf(AST_EXPR_IDENTIFIER)) {
    return name->generateType(ctx, payload);
  }

  auto ident = name->as<Identifier>();

  // Generic name ('test::Container', or really 'test_Container', since it's mangled)
  std::string id = ident->getResolvedName(ctx);

  if (!ident->genericArgs.empty()) {
    // Build concrete, mangled name ('test::Container<T>' -> 'test_Container_i32')
    std::string concrete_name = ident->getConcreteName(ctx, id);

    logger.debug("Instantiation request for '{}' ('{}')", id, concrete_name);

    // Return if already generated
    if (meta::Type::hasCustomType(concrete_name)) {
      logger.debug("Found instantiated type '{}'", concrete_name);
      return meta::Type::getCustomType(concrete_name);
    }

    assertRaiseFromNode(codegen::GenericsCache::has(id), Error(ERROR_INTERNAL_FAILURE, span, "No such generic struct: '{}'", id), this);

    auto generic = codegen::GenericsCache::get(id);

    auto concrete = generic->clone();

    auto mm = Monomorphizer(
      ctx.globalContext,
      ident->value,                              // baseName
      id,                                        // genericName
      concrete_name,                             // concreteName
      ident->getConcreteName(ctx, ident->value), // concreteUnqualifiedName
      generic->as<Struct>()->genericTypes,
      ident->genericArgs
    );

    mm.apply(concrete);

    concrete->as<Struct>()->name->value = ident->getConcreteName(ctx, ident->value);
    concrete->as<Struct>()->genericTypes.clear();

    std::shared_ptr<meta::Type> type;

    logger.debug("Instantiating Generic Type '{}' from '{}'", concrete_name, id);

    try {
      type = concrete->generateType(ctx, {});

      for (auto& method : concrete->as<Struct>()->methods) {
        logger.debug("Instantiating Generic Method '{}' for '{}' ('{}')", method->as<FnDef>()->decl->name->name(), concrete_name, id);

        auto fn = method->generateFunction(ctx, payload);

        fn->setLinkage(llvm::Function::LinkOnceODRLinkage);

        llvm::Triple target(ctx.llvm.module->getTargetTriple());

        if (target.supportsCOMDAT()) {
          fn->setComdat(ctx.llvm.module->getOrInsertComdat(fn->getName())); // Needed for linux & windows linker to know which
        }
      }
    } catch (CompilationException& ex) {
      ex.error.note(generic->span, "During instantiation of '{}'", id).raiseFromNode(this);
    }

    return type;
  }

  return meta::Type::fromTypeName(ctx.globalContext, id, name->span);
}

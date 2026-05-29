#include "xcc/ast/identifier.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/util/string.h"
#include "xcc/util/util.h"

using namespace xcc;
using namespace xcc::ast;

Identifier::Identifier(
  SourceSpan               span,
  LexicalScope             lexicalScope,
  std::string              value,
  std::vector<std::string> scope,
  NodeList                 genericArgs
)
  : Node(AST_EXPR_IDENTIFIER, span, lexicalScope), value(std::move(value)), scope(std::move(scope)), genericArgs(std::move(genericArgs)) {}

std::shared_ptr<Identifier> Identifier::create(
  SourceSpan               span,
  LexicalScope             lexicalScope,
  std::string              value,
  std::vector<std::string> scope,
  NodeList                 genericArgs
) {
  return std::make_shared<Identifier>(span, lexicalScope, value, scope, genericArgs);
}

std::shared_ptr<Node> Identifier::clone() {
  return withAttrs(create(span, Node::scope, value, scope, genericArgs));
}

void Identifier::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {}

std::string Identifier::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(0, false);

  for (auto& part : scope) {
    res += part + "::";
  }

  res += value;

  if (!genericArgs.empty()) {
    res += "::<";
    for (size_t i = 0; i < genericArgs.size(); ++i) {
      res += genericArgs[i]->toString(parent, this, indent, false);
      if (i + 1 < genericArgs.size()) {
        res += ", ";
      }
    }
    res += ">";
  }

  return res;
}

std::string Identifier::name() const {
  std::string prefix;

  for (auto& part : scope) {
    prefix += part + "_";
  }

  return prefix + value;
}

std::string Identifier::prefix() const {
  std::string prefix;

  for (size_t i = 0; i < scope.size(); ++i) {
    prefix += scope[i];
    if (i + 1 < scope.size()) {
      prefix += "_";
    }
  }

  return prefix;
}

std::string Identifier::getResolvedName(codegen::ModuleContext& ctx) const {
  if (meta::Type::isBuiltIn(value)) {
    return value;
  }

  if (!scope.empty()) {
    std::string base_scope = scope[0];

    // Base scope (first link in scope chain) could be an alias
    // if so - resolve the alias and then proceed normally with
    // mangling the rest of the identifier
    if (ctx.globalContext.isAliased(base_scope)) {
      std::string resolved_base = ctx.globalContext.aliased(base_scope);

      std::string result = resolved_base;

      for (size_t i = 1; i < scope.size(); ++i) {
        result += "_" + scope[i];
      }

      result += "_" + value;

      return result;
    }

    std::string mangled_scope = str::join(scope, "_", false);

    std::string unqualified = mangled_scope + value;

    std::string current_prefix = str::join(util::pairVectorExtractFirst(ctx.globalContext.moduleStack), "_", false);

    // Try prepending current moduleStack
    std::string qualified = current_prefix + unqualified;

    // Check if anything with qualified name (current moduleStack + scope + name) exists
    if (ctx.globalContext.functions.contains(qualified)) {
      return qualified;
    }

    if (ctx.globalContext.consts.contains(qualified)) {
      return qualified;
    }

    // TODO: Globals, macros and aliases are not checked. Should they be?

    // If prepending moduleStack didn't yield any
    // existing symbols - return scope + name as-is
    return ctx.globalContext.aliased(unqualified);
  }

  return resolveSymbolName(ctx, value);
}

std::string Identifier::getConcreteName(codegen::ModuleContext& ctx, const std::string& name) const {
  std::string concrete_name = name;

  for (auto& arg : genericArgs) {
    concrete_name += "_" + arg->generateType(ctx, {})->getName();
  }

  str::replace(concrete_name, "*", "_ptr");

  return concrete_name;
}

std::shared_ptr<Type> Identifier::intoParentType() {
  auto ident = cast<Identifier>(clone());

  ident->value = ident->scope.back();
  ident->scope.pop_back();

  return Type::create(ident->span, ident->scope, ident);
}

llvm::Value * Identifier::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  auto val = generateValueWithoutLoad(ctx, payload);

  if (scope.empty() && ctx.hasLocal(value)) {
    auto meta_type = ctx.getLocalType(value);

    // Arrays decay to pointers, everything else gets loaded
    if (meta_type->isArray()) return val;
    return ctx.ir_builder->CreateLoad(meta_type->getLLVMType(ctx), val, value.c_str());
  }

  std::string search_name = getResolvedName(ctx);
  if (ctx.globalContext.hasGlobal(search_name)) {
    auto meta_type = ctx.globalContext.getGlobalType(search_name);

    if (meta_type->isArray()) return val;
    return ctx.ir_builder->CreateLoad(meta_type->getLLVMType(ctx), val, search_name.c_str());
  }

  // Functions, constants, and enums don't need to be loaded
  return val;
}

llvm::Value * Identifier::generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  // Local variables never have explicit scopes
  if (ctx.hasLocal(value)) {
    return ctx.getLocalValue(value);
  }

  if (!genericArgs.empty()) {
    return ctx.getFunction(resolveStaticMethodName(ctx, payload));
  }

  std::string search_name = getResolvedName(ctx);

  if (ctx.globalContext.hasGlobal(search_name)) {
    return ctx.globalContext.getGlobal(ctx, search_name);
  }

  if (auto constant = ctx.globalContext.getConst(search_name)) {
    return constant->generateConstant(ctx, payload);
  }

  if (auto * fn = ctx.getFunction(search_name)) {
    return fn;
  }

  if (auto enum_field = checkGenerateEnum(ctx, payload)) {
    return enum_field;
  }

  generateValueUndeclaredError(search_name);
}

llvm::Constant * Identifier::generateConstant(codegen::ModuleContext& ctx, PayloadList payload) {
  std::string search_name = getResolvedName(ctx);

  if (auto constant = ctx.globalContext.getConst(search_name)) {
    return constant->generateConstant(ctx, payload);
  }

  if (auto enum_field = checkGenerateEnum(ctx, payload)) {
    return enum_field;
  }

  Error(ERROR_NOT_CONSTANT, span, "'{}'", defaultToString()).raiseFromNode(this);
}

std::shared_ptr<meta::Type> Identifier::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  return generateTypeForValueWithoutLoad(ctx, payload);
}

std::shared_ptr<meta::Type> Identifier::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  if (ctx.hasLocal(value)) {
    return ctx.getLocalType(value);
  }

  if (!scope.empty()) {
    std::string scope_target;

    for (size_t i = 0; i < scope.size(); ++i) {
      scope_target += scope[i] + (i == scope.size() - 1 ? "" : "_");
    }

    std::string actual_scope_name = ctx.globalContext.aliased(scope_target);

    bool is_generic = codegen::GenericsCache::has(actual_scope_name);

    if (auto _struct = meta::Type::getCustomType(actual_scope_name)) {
      is_generic |= _struct->isStruct();
    }

    if (is_generic) {
      return ctx.globalContext.getMetaFunctionType(resolveStaticMethodName(ctx, payload));
    }
  }

  std::string search_name = getResolvedName(ctx);

  if (ctx.globalContext.hasGlobal(search_name)) {
    return ctx.globalContext.getGlobalType(search_name);
  }

  if (auto constant = ctx.globalContext.getConst(search_name)) {
    return constant->generateType(ctx, payload);
  }

  if (auto meta_fn = ctx.globalContext.getMetaFunction(search_name)) {
    return meta_fn->decl->generateType(ctx, payload);
  }

  if (!scope.empty()) {
    std::string enum_target;

    for (size_t i = 0; i < scope.size(); ++i) {
      enum_target += scope[i] + (i == scope.size() - 1 ? "" : "_");
    }

    std::string actual_enum_name = resolveSymbolName(ctx, enum_target);

    auto _enum = meta::Type::getCustomType(actual_enum_name);

    if (_enum && _enum->isEnum()) {
      assertRaiseFromNode(_enum->hasEnumElement(value), Error(ERROR_ENUM_NO_MEMBER, span, "'{}' in enum '{}'", value, scope.back()), this);
      return _enum;
    }
  }

  if (ctx.hasPhantom(value)) {
    return ctx.getPhantomType(value);
  }

  generateValueUndeclaredError(search_name);
}

llvm::Constant * Identifier::checkGenerateEnum(codegen::ModuleContext& ctx, PayloadList payload) {
  if (scope.empty()) {
    return nullptr;
  }

  // Reconstruct the explicit enum target name
  std::string enum_target;

  for (size_t i = 0; i < scope.size(); ++i) {
    enum_target += scope[i] + (i == scope.size() - 1 ? "" : "_");
  }

  std::string actual_enum_name = resolveSymbolName(ctx, enum_target);

  if (meta::Type::hasCustomType(actual_enum_name)) {
    auto _enum = meta::Type::getCustomType(actual_enum_name);

    if (_enum->hasEnumElement(value)) {
      return llvm::ConstantInt::get(_enum->getBaseType()->getLLVMType(ctx), _enum->getEnumElement(value).value);
    }
  }

  return nullptr;
}

std::string Identifier::resolveStaticMethodName(codegen::ModuleContext& ctx, PayloadList payload) {
  std::string generic_struct_name;

  for (size_t i = 0; i < scope.size(); ++i) {
    generic_struct_name += scope[i];

    if (i + 1 < scope.size()) {
      generic_struct_name += "_";
    }
  }

  generic_struct_name = ctx.globalContext.aliased(generic_struct_name);

  assertRaiseFromNode(codegen::GenericsCache::has(generic_struct_name), Error(ERROR_NO_SUCH_GENERIC_TYPE, span), this);

  auto generic_struct = codegen::GenericsCache::get(generic_struct_name)->as<Struct>();

  // Pad generic type with default generic param values, if missing
  for (size_t i = genericArgs.size(); i < generic_struct->genericParams.size(); ++i) {
    auto& param = generic_struct->genericParams[i];

    assertRaiseFromNode(param.default_value.get(),
      Error(ERROR_GENERIC_COUNT_MISMATCH, span, "Missing required generic argument '{}' for struct '{}'", param.name->defaultToString(), name()), this);

    // Clone the default type and push it into the identifier's arguments
    genericArgs.push_back(param.default_value->clone());
  }

  auto concrete_struct_name = getConcreteName(ctx, generic_struct_name);

  if (!meta::Type::getCustomType(concrete_struct_name)) {
    intoParentType()->generateType(ctx, payload);
  }

  auto method_name = concrete_struct_name + "_" + value;

  if (ctx.getFunction(method_name)) {
    return method_name;
  }

  Error(ERROR_UNKNOWN_FUNCTION, span, "'{}'", method_name).raiseFromNode(this);
}

void Identifier::generateValueUndeclaredError(const std::string& search_name) {
  auto err = Error(ERROR_UNDECLARED_VALUE, span, "'{}'", defaultToString());

  if (meta::Type::hasCustomType(search_name)) {
    err = err.hint(span.pointPastLast(), "{}", "A type '{}' is found, add '{{}}' if you meant to use it as an initializer", defaultToString());
  }

  err.raiseFromNode(this);
}

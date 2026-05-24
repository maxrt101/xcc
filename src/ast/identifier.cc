#include "xcc/ast/identifier.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/util/string.h"

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
    std::string explicit_name;

    for (const auto& s : scope) {
      explicit_name += s + "_";
    }

    return ctx.globalContext.aliased(explicit_name + value);
  }

  return resolveSymbolName(ctx, value);
}

std::string Identifier::getConcreteName(codegen::ModuleContext& ctx, const std::string& name) const {
  std::string concrete_name = name;

  for (auto& arg : genericArgs) {
    concrete_name += "_" + arg->generateType(ctx, {})->toString();
  }

  util::strreplace(concrete_name, "*", "_ptr");

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
  if (!genericArgs.empty()) {
    return ctx.getFunction(resolveStaticMethodName(ctx, payload));
  }

  // Local variables never have explicit scopes
  if (scope.empty() && ctx.hasLocal(value)) {
    return ctx.getLocalValue(value);
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

  Error(ERROR_UNDECLARED_VALUE, span, "'{}'", toString(nullptr, nullptr, 0, false)).raiseFromNode(this);
}

llvm::Constant * Identifier::generateConstant(codegen::ModuleContext& ctx, PayloadList payload) {
  std::string search_name = getResolvedName(ctx);

  if (auto constant = ctx.globalContext.getConst(search_name)) {
    return constant->generateConstant(ctx, payload);
  }

  if (auto enum_field = checkGenerateEnum(ctx, payload)) {
    return enum_field;
  }

  Error(ERROR_NOT_CONSTANT, span, "'{}'", toString(nullptr, nullptr, 0, false)).raiseFromNode(this);
}

std::shared_ptr<meta::Type> Identifier::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  return generateTypeForValueWithoutLoad(ctx, payload);
}

std::shared_ptr<meta::Type> Identifier::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  if (!genericArgs.empty()) {
    return ctx.globalContext.getMetaFunctionType(resolveStaticMethodName(ctx, payload));
  }

  if (scope.empty()) {
    if (ctx.hasPhantom(value)) return ctx.getPhantomType(value);
    if (ctx.hasLocal(value)) return ctx.getLocalType(value);
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

    if (meta::Type::hasCustomType(actual_enum_name)) {
      auto _enum = meta::Type::getCustomType(actual_enum_name);
      assertRaiseFromNode(_enum->hasEnumElement(value), Error(ERROR_ENUM_NO_MEMBER, span, "'{}' in enum '{}'", value, scope.back()), this);
      return _enum;
    }
  }

  Error(ERROR_UNDECLARED_VALUE, span, "'{}'", toString(nullptr, nullptr, 0, false)).raiseFromNode(this);
}

llvm::Constant * Identifier::checkGenerateEnum(codegen::ModuleContext& ctx, PayloadList payload) {
  if (scope.empty()) return nullptr;

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

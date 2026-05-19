/**
 * Note on name resolution:
 *
 * All of Identifier::generate* functions follow these precedence rules for resolving lvalues:
 *  - Local variables
 *  - Global variables
 *  - Constants
 *  - Functions (as a first-class value)
 *  - Enum values
 *
 * Constants and enum values are resolved differently:
 * For constants, firstly the scoped identifier (scope + name), secondly module scoped identifier (current module
 * scope + name)
 * For enum values firstly the scope, secondly module-scoped scope (current module scope + identifier scope)
 */

#include "xcc/ast/identifier.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc;
using namespace xcc::ast;

Identifier::Identifier(SourceSpan span, std::string value, std::vector<std::string> scope)
  : Node(AST_EXPR_IDENTIFIER, span), value(std::move(value)), scope(std::move(scope)) {}

std::shared_ptr<Identifier> Identifier::create(SourceSpan span, const std::string& value, std::vector<std::string> scope) {
  return std::make_shared<Identifier>(span, value, scope);
}

std::shared_ptr<Node> Identifier::clone() {
  return withAttrs(create(span, value, scope));
}

void Identifier::visit(std::unique_ptr<codegen::GlobalContext>& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {}

std::string Identifier::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(0, false);

  for (auto& part : scope) {
    res += part + "::";
  }

  return res + value;
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

llvm::Value * Identifier::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  auto id = ctx.globalContext.aliased(name());
  auto mid = ctx.globalContext.aliased(ctx.globalContext.getModulePrefix()); // module id - only the current module scope

  if (ctx.hasLocal(id)) {
    auto local = ctx.getLocalValue(id);
    if (local->getAllocatedType()->isArrayTy()) {
      return local;
    }

    return ctx.ir_builder->CreateLoad(local->getAllocatedType(), local, id.c_str());
  }

  if (ctx.globalContext.hasGlobal(id)) {
    auto global    = ctx.globalContext.getGlobal(ctx, id);
    auto meta_type = ctx.globalContext.getGlobalType(id);

    if (meta_type->isArray()) {
      return global;
    }

    return ctx.ir_builder->CreateLoad(
      meta_type->getLLVMType(ctx),
      global,
      id.c_str()
    );
  }

  // Check const with full id (e.g. `module::constant`, where scope={module}, value=constant)
  if (auto constant = ctx.globalContext.getConst(id)) {
    return constant->generateConstant(ctx, payload);
  }

  // Check const with module id (e.g. `current_module::referenced_module::constant`,
  // where modulePrefix={current_module} scope={referenced_module}, value=constant)
  if (auto constant = ctx.globalContext.getConst(mid + value)) {
    return constant->generateConstant(ctx, payload);
  }

  if (auto * fn = ctx.getFunction(id)) {
    return fn;
  }

  if (auto enum_field = checkGenerateEnum(ctx, payload)) {
    return enum_field;
  }

  Error(ERROR_UNDECLARED_VALUE, span, "'{}'", toString(nullptr, nullptr, 0, false)).raiseFromNode(this);
}

llvm::Value * Identifier::generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  auto id = ctx.globalContext.aliased(name()); // id - fully assembled identifier (with scope prepended)
  auto pid = ctx.globalContext.aliased(prefix()); // prefix id - only the scope (used to reference an enum value)
  auto mid = ctx.globalContext.aliased(ctx.globalContext.getModulePrefix()); // module id - only the current module scope

  if (ctx.hasLocal(id)) {
    return ctx.getLocalValue(id);
  }

  if (ctx.globalContext.hasGlobal(id)) {
    return ctx.globalContext.getGlobal(ctx, id);
  }

  // Check const with full id (e.g. `module::constant`, where scope={module}, value=constant)
  if (auto constant = ctx.globalContext.getConst(id)) {
    return constant->generateConstant(ctx, payload);
  }

  // Check const with module id (e.g. `current_module::referenced_module::constant`,
  // where modulePrefix={current_module} scope={referenced_module}, value=constant)
  if (auto constant = ctx.globalContext.getConst(mid + value)) {
    return constant->generateConstant(ctx, payload);
  }

  if (auto * fn = ctx.getFunction(id)) {
    return fn;
  }

  if (auto enum_field = checkGenerateEnum(ctx, payload)) {
    return enum_field;
  }

  Error(ERROR_UNDECLARED_VALUE, span, "'{}'", toString(nullptr, nullptr, 0, false)).raiseFromNode(this);
}

llvm::Constant * Identifier::generateConstant(codegen::ModuleContext& ctx, PayloadList payload) {
  auto id = ctx.globalContext.aliased(name()); // id - fully assembled identifier (with scope prepended)
  auto pid = ctx.globalContext.aliased(prefix()); // prefix id - only the scope (used to reference an enum value)
  auto mid = ctx.globalContext.aliased(ctx.globalContext.getModulePrefix() + prefix()); // module id - current module scope + id scope

  // Check const with full id (e.g. `module::constant`, where scope={module}, value=constant)
  if (auto constant = ctx.globalContext.getConst(id)) {
    return constant->generateConstant(ctx, payload);
  }

  // Check const with module id (e.g. `current_module::referenced_module::constant`,
  // where modulePrefix={current_module} scope={referenced_module}, value=constant)
  if (auto constant = ctx.globalContext.getConst(mid + value)) {
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
  auto id = ctx.globalContext.aliased(name()); // id - fully assembled identifier (with scope prepended)
  auto pid = ctx.globalContext.aliased(prefix()); // prefix id - only the scope (used to reference an enum value)
  auto mid = ctx.globalContext.aliased(ctx.globalContext.getModulePrefix() + prefix()); // module id - current module scope + id scope

  if (ctx.hasPhantom(id)) {
    return ctx.getPhantomType(id);
  }

  if (ctx.hasLocal(id)) {
    return ctx.getLocalType(id);
  }

  if (ctx.globalContext.hasGlobal(id)) {
    return ctx.globalContext.getGlobalType(id);
  }

  // Check const with full id (e.g. `module::constant`, where scope={module}, value=constant)
  if (auto constant = ctx.globalContext.getConst(id)) {
    return constant->generateType(ctx, payload);
  }

  // Check const with module id (e.g. `current_module::referenced_module::constant`,
  // where modulePrefix={current_module} scope={referenced_module}, value=constant)
  if (auto constant = ctx.globalContext.getConst(mid + value)) {
    return constant->generateType(ctx, payload);
  }

  if (auto meta_fn = ctx.globalContext.getMetaFunction(id)) {
    return meta_fn->decl->generateType(ctx, payload);
  }

  // Check const with prefixed id (e.g. `Enum` for `Enum::Value`, where scope={Enum}, value=Value)
  if (meta::Type::hasCustomType(pid)) {
    auto _enum = meta::Type::getCustomType(pid);

    assertRaiseFromNode(_enum->hasEnumElement(value),
      Error(ERROR_ENUM_NO_MEMBER, span, "'{}' in enum '{}'", value, scope.back()), this);

    return _enum;
  }

  // Check const with module prefixed id (e.g. `module::Enum` for `Enum::Value`,
  // where modulePrefix={module} scope={Enum}, value=Value)
  if (meta::Type::hasCustomType(mid)) {
    auto _enum = meta::Type::getCustomType(mid);

    assertRaiseFromNode(_enum->hasEnumElement(value),
      Error(ERROR_ENUM_NO_MEMBER, span, "'{}' in enum '{}'", value, scope.back()), this);

    return _enum;
  }

  // TODO: Is needed?
#if 0
  try {
    if (auto t = meta::Type::fromTypeName(ctx.globalContext, name(), span)) {
      return t;
    }
  } catch (CompilationException&) {
    // Ignore
  }
#endif

  Error(ERROR_UNDECLARED_VALUE, span, "'{}'", toString(nullptr, nullptr, 0, false)).raiseFromNode(this);
}

llvm::Constant * Identifier::checkGenerateEnum(codegen::ModuleContext& ctx, PayloadList payload) {
  auto pid = ctx.globalContext.aliased(prefix()); // prefix id - only the scope (used to reference an enum value)
  auto mid = ctx.globalContext.aliased(ctx.globalContext.getModulePrefix() + prefix()); // module id - current module scope + id scope

  // Check const with prefixed id (e.g. `Enum` for `Enum::Value`, where scope={Enum}, value=Value)
  if (meta::Type::hasCustomType(pid)) {
    auto _enum = meta::Type::getCustomType(pid);

    assertRaiseFromNode(_enum->hasEnumElement(value),
      Error(ERROR_ENUM_NO_MEMBER, span, "'{}' in enum '{}'", value, scope.back()), this);

    return llvm::ConstantInt::get(_enum->getBaseType()->getLLVMType(ctx), _enum->getEnumElement(value).value);
  }

  // Check const with module prefixed id (e.g. `module::Enum` for `Enum::Value`,
  // where modulePrefix={module} scope={Enum}, value=Value)
  if (meta::Type::hasCustomType(mid)) {
    auto _enum = meta::Type::getCustomType(mid);

    assertRaiseFromNode(_enum->hasEnumElement(value),
      Error(ERROR_ENUM_NO_MEMBER, span, "'{}' in enum '{}'", value, scope.back()), this);

    return llvm::ConstantInt::get(_enum->getBaseType()->getLLVMType(ctx), _enum->getEnumElement(value).value);
  }

  return nullptr;
}

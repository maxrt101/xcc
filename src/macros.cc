#include "xcc/xcc.h"
#include "xcc/ast.h"
#include "xcc/codegen.h"
#include "xcc/util/log.h"

using namespace xcc;
using namespace xcc::ast;

static auto logger = util::log::Logger("MACROS");

static std::shared_ptr<meta::Type> evalType(Macro::NativeContext& ctx, codegen::ModuleContext& mod, std::shared_ptr<Node> node) {
  std::shared_ptr<meta::Type> type;

  // Try to generate type using standard method
  try {
    type = node->generateType(mod, {});
  } catch (CodegenException&) {
    // If failed - other ways to generate type require the node to be an identifier
    if (!node->is(AST_EXPR_IDENTIFIER)) {
      throw;
    }

    // Ignore
  }

  auto id = node->as<Identifier>()->name();

  if (!type) {
    // If it's a constant or custom type disguised as Identifier, try to get it
    try {
      type = meta::Type::fromTypeName(ctx.global, id);
    } catch (CodegenException&) {
      // ignore
    }
  }

  // Try to look up a variable in traversed by macro resolver vardecls
  if (!type && ctx.vardecls.contains(id)) {
    type = ctx.vardecls[id]->generateType(mod, {});
  }

  // Try to look up a variable in traversed by macro resolver fndecls
  if (!type && ctx.fndecls.contains(id)) {
    type = ctx.fndecls[id]->generateType(mod, {});
  }

  // Try to look up a arg in traversed by macro resolver fndecl
  if (!type && ctx.args.contains(id)) {
    type = ctx.args[id]->generateType(mod, {});
  }

  assertThrow(bool(type), CodegenException("Cannot evaluate expression's type"));

  return type;
}

std::string getStr(std::shared_ptr<Node> node) {
  if (node->is(AST_EXPR_IDENTIFIER)) {
    return node->as<Identifier>()->name();
  }

  if (node->is(AST_EXPR_STRING)) {
    return node->as<String>()->value;
  }

  if (node->is(AST_EXPR_NUMBER)) {
    auto num = node->as<Number>();
    return num->tag == Number::INTEGER
      ? std::format("{}", num->value.integer)
      : std::format("{}", num->value.floating);
  }

  return "";
}

static std::shared_ptr<Macro> createNativeMacro(std::string name, std::vector<std::string> args, Macro::NativeFn fn) {
  std::vector<std::shared_ptr<Identifier>> id_args;

  for (auto& arg : args) {
    id_args.push_back(Identifier::create(arg));
  }

  return Macro::createNative(Identifier::create(name), id_args, fn);
}

static std::shared_ptr<Node> xcc_macro_cat(Macro::NativeContext& ctx, std::shared_ptr<MacroCall> call) {
  return Identifier::create(getStr(call->args[0]) + getStr(call->args[1]));
}

static std::shared_ptr<Node> xcc_macro_sizeof(Macro::NativeContext& ctx, std::shared_ptr<MacroCall> call) {
  auto mod = ctx.global.createModule("<eval>");

  std::shared_ptr<meta::Type> type = evalType(ctx, *mod, call->args[0]);

  auto dl   = mod->llvm.module->getDataLayout();
  auto size = dl.getTypeAllocSize(type->getLLVMType(*mod));

  return Number::createInteger(size);
}

static std::shared_ptr<Node> xcc_macro_typeof(Macro::NativeContext& ctx, std::shared_ptr<MacroCall> call) {
  auto mod  = ctx.global.createModule("<eval>");

  std::shared_ptr<meta::Type> type = evalType(ctx, *mod, call->args[0]);

  return type->toAst();
}

static std::shared_ptr<Node> xcc_macro_is_same(Macro::NativeContext& ctx, std::shared_ptr<MacroCall> call) {
  auto mod  = ctx.global.createModule("<eval>");

  std::shared_ptr<meta::Type> type1 = evalType(ctx, *mod, call->args[0]);
  std::shared_ptr<meta::Type> type2 = evalType(ctx, *mod, call->args[1]);

  logger.warn("is_same! a={} b={} {}", type1->toString(), type2->toString(), *type1 == *type2);

  return Number::createInteger(*type1 == *type2 ? 1 : 0);
}

static std::shared_ptr<Node> xcc_macro_str(Macro::NativeContext& ctx, std::shared_ptr<MacroCall> call) {}

static std::shared_ptr<Node> xcc_macro_add(Macro::NativeContext& ctx, std::shared_ptr<MacroCall> call) {}

static std::shared_ptr<Node> xcc_macro_sub(Macro::NativeContext& ctx, std::shared_ptr<MacroCall> call) {}

static std::shared_ptr<Node> xcc_macro_inc(Macro::NativeContext& ctx, std::shared_ptr<MacroCall> call) {}

static std::shared_ptr<Node> xcc_macro_dec(Macro::NativeContext& ctx, std::shared_ptr<MacroCall> call) {}

static std::vector builtin_macros = {
  createNativeMacro("cat",     {"a", "b"}, xcc_macro_cat),
  createNativeMacro("sizeof",  {"expr"},   xcc_macro_sizeof),
  createNativeMacro("typeof",  {"expr"},   xcc_macro_typeof),
  createNativeMacro("is_same", {"a", "b"}, xcc_macro_is_same),
  createNativeMacro("str",     {"expr"},   xcc_macro_str),

  createNativeMacro("add",     {"a", "b"}, xcc_macro_add),
  createNativeMacro("sub",     {"a", "b"}, xcc_macro_sub),
  createNativeMacro("inc",     {"x"},      xcc_macro_inc),
  createNativeMacro("dec",     {"x"},      xcc_macro_dec),
};

void codegen::registerBuiltinMacros(GlobalContext& ctx) {
  for (auto& macro : builtin_macros) {
    ctx.registerMacro(macro->name->name(), macro);
  }
}
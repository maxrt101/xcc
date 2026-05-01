#include "xcc/xcc.h"
#include "xcc/ast.h"
#include "xcc/codegen.h"
#include "xcc/util/log.h"

#define __GET_NUM_VAL(__n) \
  (__n->tag == Number::FLOATING ? __n->value.floating : __n->value.integer)

#define __ARITHMETIC_UNARY_OP(__name, __ctx, __call, __op)                                                            \
  assertThrow(isOrIsLastInBlock(__call->args[0], AST_EXPR_NUMBER),                                                    \
      CodegenException("Expected number as argument to " __name));                                                    \
  auto a = getOrGetLastInBlock(__call->args[0], AST_EXPR_NUMBER)->template as<Number>();                              \
  assertThrow(a->tag == Number::INTEGER, CodegenException("Expected integer as argument to " __name));                \
  return Number::createInteger(__op a->value.integer);

#define __ARITHMETIC_BINARY_OP(__name, __ctx, __call, __op)                                                           \
  assertThrow(isOrIsLastInBlock(__call->args[0], AST_EXPR_NUMBER),                                                    \
      CodegenException("Expected number for first argument to " __name));                                             \
  assertThrow(isOrIsLastInBlock(__call->args[1], AST_EXPR_NUMBER),                                                    \
      CodegenException("Expected number for second argument to " __name));                                            \
  auto a1 = getOrGetLastInBlock(__call->args[0], AST_EXPR_NUMBER)->template as<Number>();                             \
  auto a2 = getOrGetLastInBlock(__call->args[1], AST_EXPR_NUMBER)->template as<Number>();                             \
  bool is_float = a1->tag == Number::FLOATING || a2->tag == Number::FLOATING;                                         \
  double result = __GET_NUM_VAL(a1) __op __GET_NUM_VAL(a2);                                                           \
  return is_float ? Number::createFloating(result) : Number::createInteger(result);

#define __LOGIC_BINARY_OP_STR(__name, __ctx, __call, __op)                                                            \
  assertThrow(isOrIsLastInBlock(__call->args[0], AST_EXPR_STRING),                                                    \
      CodegenException("Expected number for first argument to " __name));                                             \
  assertThrow(isOrIsLastInBlock(__call->args[1], AST_EXPR_STRING),                                                    \
      CodegenException("Expected number for second argument to " __name));                                            \
  auto a1 = getOrGetLastInBlock(__call->args[0], AST_EXPR_STRING)->template as<String>();                             \
  auto a2 = getOrGetLastInBlock(__call->args[1], AST_EXPR_STRING)->template as<String>();                             \
  return Number::createInteger(a1->value __op a2->value ? 1 : 0);

#define __LOGIC_BINARY_OP_NUM(__name, __ctx, __call, __op)                                                            \
  assertThrow(isOrIsLastInBlock(__call->args[0], AST_EXPR_NUMBER),                                                    \
      CodegenException("Expected number for first argument to " __name));                                             \
  assertThrow(isOrIsLastInBlock(__call->args[1], AST_EXPR_NUMBER),                                                    \
      CodegenException("Expected number for second argument to " __name));                                            \
  auto a1 = getOrGetLastInBlock(__call->args[0], AST_EXPR_NUMBER)->template as<Number>();                             \
  auto a2 = getOrGetLastInBlock(__call->args[1], AST_EXPR_NUMBER)->template as<Number>();                             \
  return Number::createInteger(__GET_NUM_VAL(a1) __op __GET_NUM_VAL(a2) ? 1 : 0);

#define __LOGIC_BINARY_OP(__name, __ctx, __call, __op)                                                                \
  if (isOrIsLastInBlock(__call->args[0], AST_EXPR_NUMBER)                                                             \
   && isOrIsLastInBlock(__call->args[1], AST_EXPR_NUMBER)) {                                                          \
    __LOGIC_BINARY_OP_NUM(__name, __ctx, __call, __op);                                                               \
  } else if (isOrIsLastInBlock(__call->args[0], AST_EXPR_STRING)                                                      \
          && isOrIsLastInBlock(__call->args[1], AST_EXPR_STRING)) {                                                   \
    __LOGIC_BINARY_OP_STR(__name, __ctx, __call, __op);                                                               \
  } else {                                                                                                            \
    throw CodegenException(__name " expects either 2 strings or 2 numbers");                                         \
  }

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

static std::shared_ptr<Node> xcc_macro_cat(Macro::NativeContext& ctx, std::shared_ptr<MacroCall>& call) {
  return Identifier::create(getStr(call->args[0]) + getStr(call->args[1]));
}

static std::shared_ptr<Node> xcc_macro_sizeof(Macro::NativeContext& ctx, std::shared_ptr<MacroCall>& call) {
  auto mod = ctx.global.createModule("<eval>");

  std::shared_ptr<meta::Type> type = evalType(ctx, *mod, call->args[0]);

  auto dl   = mod->llvm.module->getDataLayout();
  auto size = dl.getTypeAllocSize(type->getLLVMType(*mod));

  return Number::createInteger(size);
}

static std::shared_ptr<Node> xcc_macro_typeof(Macro::NativeContext& ctx, std::shared_ptr<MacroCall>& call) {
  auto mod  = ctx.global.createModule("<eval>");

  std::shared_ptr<meta::Type> type = evalType(ctx, *mod, call->args[0]);

  return type->toAst();
}

static std::shared_ptr<Node> xcc_macro_is_same(Macro::NativeContext& ctx, std::shared_ptr<MacroCall>& call) {
  auto mod  = ctx.global.createModule("<eval>");

  std::shared_ptr<meta::Type> type1 = evalType(ctx, *mod, call->args[0]);
  std::shared_ptr<meta::Type> type2 = evalType(ctx, *mod, call->args[1]);

  return Number::createInteger(*type1 == *type2 ? 1 : 0);
}

static std::shared_ptr<Node> xcc_macro_str(Macro::NativeContext& ctx, std::shared_ptr<MacroCall>& call) {
  return String::create(call->args[0]->toString(nullptr, call.get(), 0, false));
}

static std::shared_ptr<Node> xcc_macro_strf(Macro::NativeContext& ctx, std::shared_ptr<MacroCall>& call) {
  return String::create(call->args[0]->toString(nullptr, call.get(), 0, true));
}

static std::shared_ptr<Node> xcc_macro_int(Macro::NativeContext& ctx, std::shared_ptr<MacroCall>& call) {
  assertThrow(isOrIsLastInBlock(call->args[0], AST_EXPR_STRING), CodegenException("int! expects a string as an argument"));
  auto s = getOrGetLastInBlock(call->args[0], AST_EXPR_STRING)->as<String>();

  if (s->value.find('.') != std::string::npos) {
    return Number::createFloating(std::stod(s->value));
  }

  auto res = util::determineBase(s->value);

  return Number::createInteger(std::stol(res.value, nullptr, res.base));
}

static std::shared_ptr<Node> xcc_macro_cond(Macro::NativeContext& ctx, std::shared_ptr<MacroCall>& call) {
  assertThrow(isOrIsLastInBlock(call->args[0], AST_EXPR_NUMBER), CodegenException("cond! expects a number as first argument"));

  auto cond = getOrGetLastInBlock(call->args[0], AST_EXPR_NUMBER)->as<Number>();
  assertThrow(cond->tag == Number::INTEGER, CodegenException("Expected integer as first argument to cond!"));

  return (cond->value.integer ? call->args[1] : call->args[2])->clone();
}

static std::shared_ptr<Node> xcc_macro_repeat(Macro::NativeContext& ctx, std::shared_ptr<MacroCall>& call) {
  assertThrow(isOrIsLastInBlock(call->args[0], AST_EXPR_NUMBER),     CodegenException("repeat! expects a number as first argument"));
  assertThrow(isOrIsLastInBlock(call->args[1], AST_EXPR_IDENTIFIER), CodegenException("repeat! expects a variable name as second argument"));

  auto n = getOrGetLastInBlock(call->args[0], AST_EXPR_NUMBER)->as<Number>();
  assertThrow(n->tag == Number::INTEGER, CodegenException("Expected integer as first argument to repeat!"));

  auto var = getOrGetLastInBlock(call->args[1], AST_EXPR_IDENTIFIER)->as<Identifier>()->name();

  auto block = Block::create({});

  for (size_t i = 0; i < n->value.integer; ++i) {
    block->body.push_back(call->args[2]->clone());
    subtree::replaceIdentifierWithNode(block->body.back(), var, Number::createInteger(i));
  }

  return block;
}

static std::vector builtin_macros = {
  createNativeMacro("cat",     {"a", "b"},               xcc_macro_cat),
  createNativeMacro("sizeof",  {"expr"},                 xcc_macro_sizeof),
  createNativeMacro("typeof",  {"expr"},                 xcc_macro_typeof),
  createNativeMacro("is_same", {"a", "b"},               xcc_macro_is_same),
  createNativeMacro("str",     {"expr"},                 xcc_macro_str),
  createNativeMacro("strf",    {"expr"},                 xcc_macro_strf),
  createNativeMacro("int",     {"expr"},                 xcc_macro_int),
  createNativeMacro("cond",    {"cond", "then", "else"}, xcc_macro_cond),
  createNativeMacro("repeat",  {"n", "var", "expr"},     xcc_macro_repeat),

  createNativeMacro("inc", {"x"},      [](auto& ctx, auto& call) { __ARITHMETIC_UNARY_OP("inc!",  ctx, call, ++); }),
  createNativeMacro("dec", {"x"},      [](auto& ctx, auto& call) { __ARITHMETIC_UNARY_OP("dec!",  ctx, call, --); }),
  createNativeMacro("add", {"a", "b"}, [](auto& ctx, auto& call) { __ARITHMETIC_BINARY_OP("add!", ctx, call, +);  }),
  createNativeMacro("sub", {"a", "b"}, [](auto& ctx, auto& call) { __ARITHMETIC_BINARY_OP("sub!", ctx, call, -);  }),
  createNativeMacro("mul", {"a", "b"}, [](auto& ctx, auto& call) { __ARITHMETIC_BINARY_OP("mul!", ctx, call, *);  }),
  createNativeMacro("div", {"a", "b"}, [](auto& ctx, auto& call) { __ARITHMETIC_BINARY_OP("div!", ctx, call, /);  }),
  createNativeMacro("eq",  {"a", "b"}, [](auto& ctx, auto& call) { __LOGIC_BINARY_OP(     "eq!",  ctx, call, ==); }),
  createNativeMacro("ne",  {"a", "b"}, [](auto& ctx, auto& call) { __LOGIC_BINARY_OP(     "ne!",  ctx, call, !=); }),
  createNativeMacro("lt",  {"a", "b"}, [](auto& ctx, auto& call) { __LOGIC_BINARY_OP_NUM( "lt!",  ctx, call, <);  }),
  createNativeMacro("le",  {"a", "b"}, [](auto& ctx, auto& call) { __LOGIC_BINARY_OP_NUM( "le!",  ctx, call, <=); }),
  createNativeMacro("gt",  {"a", "b"}, [](auto& ctx, auto& call) { __LOGIC_BINARY_OP_NUM( "gt!",  ctx, call, >);  }),
  createNativeMacro("ge",  {"a", "b"}, [](auto& ctx, auto& call) { __LOGIC_BINARY_OP_NUM( "ge!",  ctx, call, >=); }),
  createNativeMacro("and", {"a", "b"}, [](auto& ctx, auto& call) { __LOGIC_BINARY_OP_NUM( "and!", ctx, call, &&); }),
  createNativeMacro("or", {"a", "b"},  [](auto& ctx, auto& call) { __LOGIC_BINARY_OP_NUM( "or!",  ctx, call, ||); }),
  createNativeMacro("not", {"expr"},   [](auto& ctx, auto& call) { __ARITHMETIC_UNARY_OP( "not!", ctx, call, !);  }),
};

void codegen::registerBuiltinMacros(GlobalContext& ctx) {
  for (auto& macro : builtin_macros) {
    ctx.registerMacro(macro->name->name(), macro);
  }
}
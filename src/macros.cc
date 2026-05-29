#include "xcc/xcc.h"
#include "xcc/ast.h"
#include "xcc/codegen.h"
#include "xcc/util/log.h"
#include "xcc/util/util.h"

#define __GET_NUM_VAL(__n) \
  (__n->tag == Number::FLOATING ? __n->value.floating : __n->value.integer)

#define __ARITHMETIC_UNARY_OP(__name, __ctx, __call, __op)                                                            \
  assertRaise(isOrIsLastInBlock(__call->args[0], AST_EXPR_NUMBER),                                                    \
      Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, __call->args[0]->span, "Expected number as argument to " __name));    \
  auto a = getOrGetLastInBlock(__call->args[0], AST_EXPR_NUMBER)->template as<Number>();                              \
  assertRaise(a->tag == Number::INTEGER,                                                                              \
      Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, __call->args[0]->span, "Expected integer as argument to " __name));   \
  return Number::createInteger(__call->span, __call->scope, __op a->value.integer);

#define __ARITHMETIC_BINARY_OP(__name, __ctx, __call, __op)                                                           \
  assertRaise(isOrIsLastInBlock(__call->args[0], AST_EXPR_NUMBER),                                                    \
      Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, __call->args[0]->span,                                                \
          "Expected number for first argument to " __name));                                                          \
  assertRaise(isOrIsLastInBlock(__call->args[1], AST_EXPR_NUMBER),                                                    \
      Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, __call->args[1]->span,                                                \
          "Expected number for second argument to " __name));                                                         \
  auto a1 = getOrGetLastInBlock(__call->args[0], AST_EXPR_NUMBER)->template as<Number>();                             \
  auto a2 = getOrGetLastInBlock(__call->args[1], AST_EXPR_NUMBER)->template as<Number>();                             \
  bool is_float = a1->tag == Number::FLOATING || a2->tag == Number::FLOATING;                                         \
  double result = __GET_NUM_VAL(a1) __op __GET_NUM_VAL(a2);                                                           \
  return is_float                                                                                                     \
    ? Number::createFloating(__call->span, __call->scope, result)                                                     \
    : Number::createInteger(__call->span, __call->scope, result);

#define __LOGIC_BINARY_OP_STR(__name, __ctx, __call, __op)                                                            \
  assertRaise(isOrIsLastInBlock(__call->args[0], AST_EXPR_STRING),                                                    \
      Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, __call->args[0]->span,                                                \
          "Expected number for first argument to " __name));                                                          \
  assertRaise(isOrIsLastInBlock(__call->args[1], AST_EXPR_STRING),                                                    \
      Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, __call->args[1]->span,                                                \
          "Expected number for second argument to " __name));                                                         \
  auto a1 = getOrGetLastInBlock(__call->args[0], AST_EXPR_STRING)->template as<String>();                             \
  auto a2 = getOrGetLastInBlock(__call->args[1], AST_EXPR_STRING)->template as<String>();                             \
  return Number::createInteger(__call->span, __call->scope, a1->value __op a2->value ? 1 : 0);

#define __LOGIC_BINARY_OP_NUM(__name, __ctx, __call, __op)                                                            \
  assertRaise(isOrIsLastInBlock(__call->args[0], AST_EXPR_NUMBER),                                                    \
      Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, __call->args[0]->span,                                                \
          "Expected number for first argument to " __name));                                                          \
  assertRaise(isOrIsLastInBlock(__call->args[1], AST_EXPR_NUMBER),                                                    \
      Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, __call->args[1]->span,                                                \
          "Expected number for second argument to " __name));                                                         \
  auto a1 = getOrGetLastInBlock(__call->args[0], AST_EXPR_NUMBER)->template as<Number>();                             \
  auto a2 = getOrGetLastInBlock(__call->args[1], AST_EXPR_NUMBER)->template as<Number>();                             \
  return Number::createInteger(__call->span, __call->scope, __GET_NUM_VAL(a1) __op __GET_NUM_VAL(a2) ? 1 : 0);

#define __LOGIC_BINARY_OP(__name, __ctx, __call, __op)                                                                \
  if (isOrIsLastInBlock(__call->args[0], AST_EXPR_NUMBER)                                                             \
   && isOrIsLastInBlock(__call->args[1], AST_EXPR_NUMBER)) {                                                          \
    __LOGIC_BINARY_OP_NUM(__name, __ctx, __call, __op);                                                               \
  } else if (isOrIsLastInBlock(__call->args[0], AST_EXPR_STRING)                                                      \
          && isOrIsLastInBlock(__call->args[1], AST_EXPR_STRING)) {                                                   \
    __LOGIC_BINARY_OP_STR(__name, __ctx, __call, __op);                                                               \
  } else {                                                                                                            \
    Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, __call->span, __name " expects either 2 strings or 2 numbers")          \
        .raise();                                                                                                     \
  }

using namespace xcc;
using namespace xcc::ast;

static auto& logger = log::Logger::get("MACROS");

struct FormatModifier {
  bool isHexLower = false;
  bool isHexUpper = false;
  bool neg        = false;
  bool dot        = false;
  std::string intPrecision, floatPrecision;

  [[nodiscard]] std::string consideringHex(const std::string& base) const {
    if (isHexLower) return "x";
    if (isHexUpper) return "X";
    return base;
  }

  [[nodiscard]] std::string getIntPrec() const {
    return intPrecision.empty() ? "" : intPrecision;
  }

  [[nodiscard]] std::string getStrPad() const {
    return intPrecision.empty() ? "" : ((neg ? "-" : "") + intPrecision);
  }

  [[nodiscard]] std::string getFloatPrec() const {
    return (intPrecision.empty() ? "" : intPrecision) + (dot ? "." : "") + (floatPrecision.empty() ? "" : floatPrecision);
  }

  static FormatModifier fromString(const std::string& str) {
    FormatModifier res;

    size_t index = 0;

    while (index < str.size()) {
      if (isdigit(str[index])) {
        if (res.dot) {
          res.floatPrecision += str[index];
        } else {
          res.intPrecision += str[index];
        }
      }

      if (str[index] == '-') {
        res.neg = true;
      }

      if (str[index] == '.') {
        res.dot = true;
      }

      if (str[index] == 'x') {
        res.isHexLower = true;
      }

      if (str[index] == 'X') {
        res.isHexUpper = true;
      }

      index++;
    }

    return res;
  }
};

static std::shared_ptr<meta::Type> evalType(codegen::GlobalContext& global, codegen::ModuleContext& mod, std::shared_ptr<Node> node) {
  // Try to generate type using standard method
  try {
    return node->generateType(mod, {});
  } catch (CompilationException&) {
    // If failed - other ways to generate type require the node to be an identifier
    if (!node->is(AST_EXPR_IDENTIFIER)) {
      throw;
    }

    // Ignore
  }

  auto id = node->as<Identifier>()->name();

  try {
    return meta::Type::fromTypeName(global, id, node->span);
  } catch (CompilationException& e) {
    // ignore
  }

   Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, node->span, "Cannot evaluate expression's type").raise();
}

/**
 * Modifiers:
 * {}      - auto determine
 * {x} {X} - print hex
 * {.3}    - float precision
 * {.03}   - float precision zero-pad
 * {3}     - int precision
 * {03}    - int precision zero-pad
 * {03}    - string left space-pad
 * {-03}   - string right space-pad
 * {T}     - type
 * {%...}  - passthrough to printf
 *
 * @param global
 * @param mod
 * @param node
 * @param modifierString
 * @return
 */
static std::string printfSpecifierFromNode(
  codegen::GlobalContext& global,
  codegen::ModuleContext& mod,
  std::shared_ptr<Node>&  node,
  const std::string&      modifierString
) {
  if (modifierString.starts_with("%")) {
    return modifierString;
  }

  auto type = evalType(global, mod, node);

  if (modifierString == "T") {
    node = String::create(node->span, node->scope, type->toString());
    return "%s";
  }

  auto modifier = FormatModifier::fromString(modifierString);

  switch (type->getTag()) {
    case meta::TypeTag::BOOL:
      return "%d"; // TODO: true/false. Maybe inject { if (ARG) { "true" } else { "false" } } as arg

    case meta::TypeTag::I8:
    case meta::TypeTag::I16:
    case meta::TypeTag::I32:
      return "%" + modifier.getIntPrec() + modifier.consideringHex("d");

    case meta::TypeTag::I64:
      return "%" + modifier.getIntPrec() + "ll" + modifier.consideringHex("d");

    case meta::TypeTag::U8:
    case meta::TypeTag::U16:
    case meta::TypeTag::U32:
      return "%" + modifier.getIntPrec() + modifier.consideringHex("u");

    case meta::TypeTag::U64:
      return "%" + modifier.getIntPrec() + "ll" + modifier.consideringHex("u");

    case meta::TypeTag::ISIZE:
      return "%" + modifier.getIntPrec() + "z" + modifier.consideringHex("d");

    case meta::TypeTag::USIZE:
      return "%" + modifier.getIntPrec() + "z" + modifier.consideringHex("u");

    case meta::TypeTag::F32:
    case meta::TypeTag::F64:
      return "%" + modifier.getFloatPrec() + "f";

    case meta::TypeTag::PTR: {
      if (type->getPointedType()->isAnyOf(meta::TypeTag::I8, meta::TypeTag::U8)) {
        return "%" + modifier.getStrPad() + "s";
      }

      return "%" + modifier.getIntPrec() + "p";
    }

    case meta::TypeTag::FUNCTION:
      return "%" + modifier.getIntPrec() + "p";

    case meta::TypeTag::ENUM: {
      // TODO: Generate a call to type->getBaseType()::toString(), set specifier to %s
      return "%d";
    }

    case meta::TypeTag::ARRAY: {
      // TODO: Generate specifier to N * baseTypeSpecifier, inject destructuring
    }

    case meta::TypeTag::STRUCT: {
      // TODO: Generate specifiers for all fields with pretty formatting, inject destructuring
    }

    case meta::TypeTag::TUPLE: {
      // TODO: Generate specifiers for all fields with pretty formatting, inject destructuring
    }

    case meta::TypeTag::VOID:
    default:
      // TODO: Raise an error (ERROR_UNPRINTABLE)
      return "?";
  }
}

static std::shared_ptr<Macro> createNativeMacro(std::string name, std::vector<std::string> args, Macro::NativeFn fn, bool variadic = false) {
  std::vector<std::shared_ptr<Identifier>> id_args;

  for (auto& arg : args) {
    id_args.push_back(Identifier::create(SourceSpan::builtin(), {}, arg));
  }

  return Macro::createNative({}, Identifier::create(SourceSpan::builtin(), {}, name), id_args, fn, variadic);
}

static std::shared_ptr<Node> xcc_macro_cat(codegen::GlobalContext& global, std::shared_ptr<MacroCall>& call) {
  return Identifier::create(call->span, call->scope, call->args[0]->defaultToString() + call->args[1]->defaultToString());
}

static std::shared_ptr<Node> xcc_macro_sizeof(codegen::GlobalContext& global, std::shared_ptr<MacroCall>& call) {
  std::shared_ptr<meta::Type> type = evalType(global, *global.globalModule, call->args[0]);

  auto dl   = global.globalModule->llvm.module->getDataLayout();
  auto size = dl.getTypeAllocSize(type->getLLVMType(*global.globalModule));

  return Number::createInteger(call->span, call->scope, size);
}

static std::shared_ptr<Node> xcc_macro_typeof(codegen::GlobalContext& global, std::shared_ptr<MacroCall>& call) {
  std::shared_ptr<meta::Type> type = evalType(global, *global.globalModule, call->args[0]);

  return type->toAst(call->span);
}

static std::shared_ptr<Node> xcc_macro_is_same(codegen::GlobalContext& global, std::shared_ptr<MacroCall>& call) {
  std::shared_ptr<meta::Type> type1 = evalType(global, *global.globalModule, call->args[0]);
  std::shared_ptr<meta::Type> type2 = evalType(global, *global.globalModule, call->args[1]);

  return Number::createInteger(call->span, call->scope, *type1 == *type2 ? 1 : 0);
}

static std::shared_ptr<Node> xcc_macro_closure_type(codegen::GlobalContext& global, std::shared_ptr<MacroCall>& call) {
  std::shared_ptr<meta::Type> type = evalType(global, *global.globalModule, call->args[0]);

  return type->getClosureType()->toAst();
}

static std::shared_ptr<Node> xcc_macro_str(codegen::GlobalContext& global, std::shared_ptr<MacroCall>& call) {
  return String::create(call->span, call->scope, call->args[0]->toString(nullptr, call.get(), 0, false));
}

static std::shared_ptr<Node> xcc_macro_strf(codegen::GlobalContext& global, std::shared_ptr<MacroCall>& call) {
  return String::create(call->span, call->scope, call->args[0]->toString(nullptr, call.get(), 0, true));
}

static std::shared_ptr<Node> xcc_macro_mangle(codegen::GlobalContext& global, std::shared_ptr<MacroCall>& call) {
  auto str = call->args[0]->defaultToString();

  str::replace(str, "*", "_ptr");

  return Identifier::create(call->span, call->scope, str);
}

static std::shared_ptr<Node> xcc_macro_int(codegen::GlobalContext& global, std::shared_ptr<MacroCall>& call) {
  assertRaise(isOrIsLastInBlock(call->args[0], AST_EXPR_STRING),
      Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, call->args[0]->span, "int! expects a string as an argument"));

  auto s = getOrGetLastInBlock(call->args[0], AST_EXPR_STRING)->as<String>();

  if (s->value.find('.') != std::string::npos) {
    return Number::createFloating(call->span, call->scope, std::stod(s->value));
  }

  auto res = str::determineBase(s->value);

  return Number::createInteger(call->span, call->scope, std::stol(res.value, nullptr, res.base));
}

static std::shared_ptr<Node> xcc_macro_cond(codegen::GlobalContext& global, std::shared_ptr<MacroCall>& call) {
  assertRaise(isOrIsLastInBlock(call->args[0], AST_EXPR_NUMBER),
      Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, call->args[0]->span, "cond! expects a number as first argument"));

  auto condValue = call->args[0]->generateConstant(*global.globalModule, {});
  bool cond = false;

  if (auto condInt = llvm::dyn_cast<llvm::ConstantInt>(condValue)) {
    cond = condInt->getValue().getZExtValue();
  } else {
    Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, call->args[0]->span, "Expected a constant as first argument to cond!").raise();
  }

  auto else_branch = call->args.size() > 2 ? call->args[2] : Block::create(call->span, call->scope, {});

  // FIXME: Currently both branches are evaluated before cond! call is processed, it means that, for example
  //        conditional error! isn't possible, because it'll get evaluated either way
  return (cond ? call->args[1] : else_branch)->clone();
}

static std::shared_ptr<Node> xcc_macro_repeat(codegen::GlobalContext& global, std::shared_ptr<MacroCall>& call) {
  assertRaise(isOrIsLastInBlock(call->args[0], AST_EXPR_NUMBER),
      Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, call->args[0]->span, "repeat! expects a number as first argument"));
  assertRaise(isOrIsLastInBlock(call->args[1], AST_EXPR_IDENTIFIER),
      Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, call->args[0]->span, "repeat! expects a variable name as second argument"));

  auto n = getOrGetLastInBlock(call->args[0], AST_EXPR_NUMBER)->as<Number>();

  assertRaise(n->tag == Number::INTEGER,
      Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, call->args[0]->span, "Expected integer as first argument to repeat!"));

  auto var = getOrGetLastInBlock(call->args[1], AST_EXPR_IDENTIFIER)->as<Identifier>()->name();

  auto block = Block::create(call->span, call->scope, {});

  for (size_t i = 0; i < n->value.integer; ++i) {
    block->body.push_back(call->args[2]->clone());
    subtree::replaceIdentifierWithNode(block->body.back(), var, Number::createInteger(call->span, call->scope, i));
  }

  return block;
}

static std::shared_ptr<Node> xcc_macro_asm(codegen::GlobalContext& global, std::shared_ptr<MacroCall>& call) {
  assertRaise(isOrIsLastInBlock(call->args[0], AST_EXPR_STRING),
    Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, call->args[0]->span, "asm! expects a string as first argument"));
  assertRaise(isOrIsLastInBlock(call->args[1], AST_EXPR_STRING),
    Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, call->args[1]->span, "asm! expects a string as second argument"));

  NodeList args;

  for (size_t i = 2; i < call->args.size(); ++i) {
    args.push_back(call->args[i]);
  }

  return Asm::create(call->span, call->scope, call->args[0], call->args[1], args);
}

static std::shared_ptr<Node> xcc_macro_assert(codegen::GlobalContext& global, std::shared_ptr<MacroCall>& call) {
  auto val = call->args[0]->generateConstant(*global.globalModule, {});

  if (auto cond = llvm::dyn_cast<llvm::ConstantInt>(val)) {
    std::string message;

    if (call->args.size() > 1) {
      assertRaise(isOrIsLastInBlock(call->args[1], AST_EXPR_STRING),
        Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, call->args[1]->span, "assert! expects a string as second argument"));

      message = ": " + call->args[1]->as<String>()->value;
    }

    if (!cond->getValue().getZExtValue()) {
      Error(ERROR_ASSERTION, call->span, "'{}'{}",
        call->args[0]->defaultToString(),
        message.empty() ? "" : message
      ).raiseFromNode(call.get());
    }
  } else {
    Error(ERROR_NOT_CONSTANT, call->args[0]->span, "assert! expects a constant").raise();
  }

  // TODO: Should be Empty
  return Block::create(call->span, call->scope, {});
}

static std::shared_ptr<Node> xcc_macro_warn(codegen::GlobalContext& global, std::shared_ptr<MacroCall>& call) {
  assertRaise(isOrIsLastInBlock(call->args[0], AST_EXPR_STRING),
    Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, call->args[0]->span, "warn! expects a string as first argument"));

  Warning(WARNING_USER_WARNING, call->span, "{}", call->args[0]->as<String>()->value).emit();

  // TODO: Should be Empty
  return Block::create(call->span, call->scope, {});
}

static std::shared_ptr<Node> xcc_macro_error(codegen::GlobalContext& global, std::shared_ptr<MacroCall>& call) {
  assertRaise(isOrIsLastInBlock(call->args[0], AST_EXPR_STRING),
    Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, call->args[0]->span, "error! expects a string as first argument"));

  Error(ERROR_USER_ERROR, call->span, "{}", call->args[0]->as<String>()->value).raiseFromNode(call.get());
}

static std::shared_ptr<Node> xcc_macro_print(codegen::GlobalContext& global, std::shared_ptr<MacroCall>& call) {
  assertRaise(isOrIsLastInBlock(call->args[0], AST_EXPR_STRING),
    Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, call->args[0]->span, "print! expects a string as first argument"));

  auto fmt = call->args[0]->as<String>()->value;

  size_t arg = 1;

  std::string res_fmt;

  size_t i = 0;

  while (i < fmt.size()) {
    if (fmt[i] == '{') {
      std::string f;
      while (fmt[++i] != '}') {
        f += fmt[i];
      }

      assertRaise(arg < call->args.size(),
        Error(ERROR_MACRO_CALL_ARG_COUNT_MISMATCH, call->args[0]->span,
          "format string has more specifiers than arguments in macro call"));

      res_fmt += printfSpecifierFromNode(global, *global.globalModule, call->args[arg++], f);
    } else {
      res_fmt += fmt[i];
    }

    i++;
  }

  call->args[0] = String::create(call->args[0]->span, call->scope, res_fmt);

  return Call::create(call->span, call->scope, Identifier::create(call->span, call->scope, "printf", {"stdc", "io"}), call->args);
}

static std::shared_ptr<Node> xcc_macro_println(codegen::GlobalContext& global, std::shared_ptr<MacroCall>& call) {
  assertRaise(isOrIsLastInBlock(call->args[0], AST_EXPR_STRING),
    Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, call->args[0]->span, "print! expects a string as first argument"));

  call->args[0]->as<String>()->value += "\n";

  return xcc_macro_print(global, call);
}

static std::shared_ptr<Node> xcc_macro_include(codegen::GlobalContext& global, std::shared_ptr<MacroCall>& call) {
  assertRaise(isOrIsLastInBlock(call->args[0], AST_EXPR_STRING),
    Error(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH, call->args[0]->span, "include! expects a string as first argument"));

  auto file = FileManager::load(call->args[0]->as<String>()->value);

  auto lexer  = Lexer(file);
  auto tokens = lexer.tokenize();
  auto parser = Parser(file, tokens, true);

  return parser.parse(false);
}

static std::vector builtin_macros = {
  createNativeMacro("cat",          {"a", "b"},               xcc_macro_cat),
  createNativeMacro("sizeof",       {"expr"},                 xcc_macro_sizeof),
  createNativeMacro("typeof",       {"expr"},                 xcc_macro_typeof),
  createNativeMacro("is_same",      {"a", "b"},               xcc_macro_is_same),
  createNativeMacro("closure_type", {"expr"},                 xcc_macro_closure_type),
  createNativeMacro("str",          {"expr"},                 xcc_macro_str),
  createNativeMacro("strf",         {"expr"},                 xcc_macro_strf),
  createNativeMacro("mangle",       {"type"},                 xcc_macro_mangle),
  createNativeMacro("int",          {"expr"},                 xcc_macro_int),
  createNativeMacro("cond",         {"cond", "then"},         xcc_macro_cond, true),
  createNativeMacro("repeat",       {"n", "var", "expr"},     xcc_macro_repeat),
  createNativeMacro("asm",          {"code", "constraints"},  xcc_macro_asm, true),

  createNativeMacro("assert",       {"expr"},                 xcc_macro_assert, true),
  createNativeMacro("warn",         {"msg"},                  xcc_macro_warn),
  createNativeMacro("error",        {"msg"},                  xcc_macro_error),

  createNativeMacro("print",        {"fmt"},                  xcc_macro_print, true),
  createNativeMacro("println",      {"fmt"},                  xcc_macro_println, true),

  createNativeMacro("include",      {"path"},                 xcc_macro_include),

  // TODO: Is this needed, if constant unary/binary expressions are implemented
  createNativeMacro("inc", {"x"},      [](auto& ctx, auto& call) { __ARITHMETIC_UNARY_OP( "inc!", ctx, call, ++); }),
  createNativeMacro("dec", {"x"},      [](auto& ctx, auto& call) { __ARITHMETIC_UNARY_OP( "dec!", ctx, call, --); }),
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
  createNativeMacro("or",  {"a", "b"}, [](auto& ctx, auto& call) { __LOGIC_BINARY_OP_NUM( "or!",  ctx, call, ||); }),
  createNativeMacro("not", {"expr"},   [](auto& ctx, auto& call) { __ARITHMETIC_UNARY_OP( "not!", ctx, call, !);  }),
};

void codegen::registerBuiltinMacros(GlobalContext& ctx) {
  for (auto& macro : builtin_macros) {
    ctx.registerMacro(macro->name->name(), macro);
  }
}
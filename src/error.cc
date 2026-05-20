#include "xcc/error.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/ast/identifier.h"
#include "xcc/ast/node.h"
#include "xcc/util/fs.h"
#include "xcc/util/log.h"
#include "xcc/util/ansi.h"

using namespace xcc;

static auto logger = xcc::util::log::Logger("ERROR");

#define DESC(__err, __str) {__err, {__err, __str}}

const std::unordered_map<ErrorId, ErrorDescription> ErrorDescription::descs = {
  DESC(ERROR_RESERVED,                                   "Reserved error"),
  DESC(ERROR_MISSING_FILE,                               "Can't open file"),
  DESC(ERROR_UNEXPECTED_EOF,                             "Unexpected End Of File"),
  DESC(ERROR_MISSING_CLOSING_QUOTE,                      "Missing closing quote"),
  DESC(ERROR_MISSING_IDENTIFIER,                         "Missing identifier"),
  DESC(ERROR_INVALID_MEMBER_ACCESS,                      "Invalid member access"),
  DESC(ERROR_FN_TYPE_MISSING_OPENING_PAREN,              "Missing '(' after 'fn' in type declaration"),
  DESC(ERROR_FN_TYPE_MISSING_CLOSING_PAREN,              "Missing ')' after 'fn' arguments in type declaration"),
  DESC(ERROR_FN_TYPE_MISSING_ARROW,                      "Missing '->' after ')' in 'fn' type declaration"),
  DESC(ERROR_FN_MISSING_KEYWORD,                         "Missing 'fn' in function declaration"),
  DESC(ERROR_FN_MISSING_OPENING_PAREN,                   "Missing '(' after 'fn' in function declaration"),
  DESC(ERROR_FN_MISSING_OPENING_PAREN,                   "Missing ')' after arguments in function declaration"),
  DESC(ERROR_FN_MISSING_SEMICOLON,                       "Missing ';' after function declaration"),
  DESC(ERROR_BLOCK_MISSING_OPENING_BRACE,                "Missing '{' at the start of block"),
  DESC(ERROR_BLOCK_MISSING_CLOSING_BRACE,                "Missing '}' at the end of block"),
  DESC(ERROR_VAR_MISSING_KEYWORD,                        "Missing 'var' at the start of variable declaration"),
  DESC(ERROR_STRUCT_MISSING_KEYWORD,                     "Missing 'struct' at the start of struct declaration"),
  DESC(ERROR_STRUCT_MISSING_OPENING_BRACE,               "Missing '{' at the start of struct body declaration"),
  DESC(ERROR_STRUCT_MISSING_CLOSING_BRACE,               "Missing '{' at the end of struct body declaration"),
  DESC(ERROR_IF_MISSING_KEYWORD,                         "Missing 'if' at the start of if statement"),
  DESC(ERROR_IF_MISSING_OPENING_PAREN,                   "Missing '(' after 'if'"),
  DESC(ERROR_IF_MISSING_CLOSING_PAREN,                   "Missing ')' after if condition"),
  DESC(ERROR_FOR_MISSING_KEYWORD,                        "Missing 'for' at the start of for statement"),
  DESC(ERROR_FOR_MISSING_OPENING_PAREN,                  "Missing '(' after 'for'"),
  DESC(ERROR_FOR_MISSING_CLOSING_PAREN,                  "Missing ')' after 'for' conditions"),
  DESC(ERROR_FOR_MISSING_INIT_SEMICOLON,                 "Missing ';' after init part of 'for'"),
  DESC(ERROR_FOR_MISSING_COND_SEMICOLON,                 "Missing ';' after condition part of 'for'"),
  DESC(ERROR_WHILE_MISSING_KEYWORD,                      "Missing 'while' at the start of while statement"),
  DESC(ERROR_WHILE_MISSING_OPENING_PAREN,                "Missing '(' after 'while'"),
  DESC(ERROR_WHILE_MISSING_CLOSING_PAREN,                "Missing ')' after 'while' condition"),
  DESC(ERROR_RETURN_MISSING_KEYWORD,                     "Missing 'return' at the beginning of return statement"),
  DESC(ERROR_USE_MISSING_KEYWORD,                        "Missing 'use' at the beginning of use statement"),
  DESC(ERROR_USE_MISSING_SEMICOLON,                      "Missing ';' at the end of use statement"),
  DESC(ERROR_PATH_ATTR_ARG_MISMATCH,                     "'path' attribute expects 1 argument"),
  DESC(ERROR_PATH_ATTR_ARG_BAD_TYPE,                     "'path' attribute expect a string argument"),
  DESC(ERROR_MOD_MISSING_KEYWORD,                        "Missing 'mod' at the beginning of module declaration"),
  DESC(ERROR_MOD_MISSING_SEMICOLON,                      "Missing ';' after file-scoped module declaration"),
  DESC(ERROR_MOD_MISSING_CLOSING_BRACE,                  "Missing '}' after module body"),
  DESC(ERROR_TYPE_MISSING_KEYWORD,                       "Missing 'type' at the beginning of type declaration"),
  DESC(ERROR_TYPE_MISSING_EQUALS,                        "Missing '=' after type name at the type declaration"),
  DESC(ERROR_TYPE_MISSING_SEMICOLON,                     "Missing ';' at the end of type declaration"),
  DESC(ERROR_MACRO_MISSING_KEYWORD,                      "Missing 'macro' at the beginning of macro declaration"),
  DESC(ERROR_MACRO_MISSING_OPENING_PAREN,                "Missing '(' after 'macro' at the macro declaration"),
  DESC(ERROR_MACRO_MISSING_CLOSING_PAREN,                "Missing ')' after macro arguments at the macro declaration"),
  DESC(ERROR_INVALID_LHS_FOR_ASSIGNMENT,                 "Invalid LHS for assignment (non-lvalue)"),
  DESC(ERROR_SUBSCRIPT_MISSING_CLOSING_BRACE,            "Missing closing ']' after '[' in subscript operator"),
  DESC(ERROR_DOLLAR_MISSING_IDENTIFIER,                  "Expected identifier after '$'"),
  DESC(ERROR_NO_ENV_VARIABLE,                            "No such environment variable"),
  DESC(ERROR_LVALUE_UNEXPECTED_TOKEN,                    "Unexpected token, expected identifier or self"),
  DESC(ERROR_MACRO_CALLEE_IS_NOT_ID,                     "Macro callee can only be an identifier"),
  DESC(ERROR_MACRO_CALL_MISSING_OPENING_PAREN,           "Expected '(' after macro name in macro call"),
  DESC(ERROR_MACRO_CALL_MISSING_CLOSING_PAREN,           "Expected ')' after macro arguments in macro call"),
  DESC(ERROR_FN_CALL_MISSING_OPENING_PAREN,              "Expected '(' after function name in function call"),
  DESC(ERROR_FN_CALL_MISSING_CLOSING_PAREN,              "Expected ')' after function arguments in function call"),
  DESC(ERROR_ATTR_MISSING_OPENING_BRACKET,               "Expected '[' at the beginning of attribute list"),
  DESC(ERROR_ATTR_MISSING_CLOSING_BRACKET,               "Expected ']' after attribute list"),
  DESC(ERROR_ATTR_MISSING_CLOSING_PAREN,                 "Expected ')' after attribute parameters"),
  DESC(ERROR_VARDECL_MISSING_SEMICOLON,                  "Expected ';' after variable declaration in global scope"),
  DESC(ERROR_TOP_LEVEL_UNEXPECTED_TOKEN,                 "Unexpected token at top-level scope"),
  DESC(ERROR_LLVM_ERROR,                                 "LLVM Error"),
  DESC(ERROR_UNKNOWN_GLOBAL_VARIABLE,                    "Unknown global variable"),
  DESC(ERROR_UNKNOWN_FUNCTION,                           "Can't find function"),
  DESC(ERROR_UNKNOWN_SYMBOL,                             "Can't find symbol"),
  DESC(ERROR_INVALID_CAST,                               "Invalid cast"),
  DESC(ERROR_MACRO_CALL_ARG_COUNT_MISMATCH,              "Argument count mismatch for macro call"),
  DESC(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH,               "Unexpected argument type for macro call"),
  DESC(ERROR_INVALID_TYPE,                               "Invalid type"),
  DESC(ERROR_INVALID_TOKEN_FOR_CONTEXT,                  "Unexpected token in current context"),
  DESC(ERROR_UNKNOWN_MACRO,                              "Can't find macro"),
  DESC(ERROR_INVALID_ASSIGNMENT_OP,                      "Invalid operation for assignment"),
  DESC(ERROR_UNKNOWN_VARIABLE,                           "Unknown variable"),
  DESC(ERROR_UNKNOWN_BIN_OP_OR_TYPE,                     "Unsupported binary expression operator or type"),
  DESC(ERROR_INTERNAL_UNEXPECTED_NULL,                   "(INTERNAL) Unexpected NULL value was received"),
  DESC(ERROR_FN_CALL_ARG_COUNT_MISMATCH,                 "Argument count mismatch for function call"),
  DESC(ERROR_FN_CALL_ARG_TYPE_MISMATCH,                  "Unexpected argument type for function call"),
  DESC(ERROR_INTERNAL_FAILURE,                           "(INTERNAL) Failed to perform action"),
  DESC(ERROR_EXPR_NOT_CALLABLE,                          "Expression is no callable"),
  DESC(ERROR_UNKNOWN_METHOD,                             "Unknown method"),
  DESC(ERROR_INTERNAL_UNEXPECTED_NODE,                   "Unexpected node"),
  DESC(ERROR_UNDECLARED_VALUE,                           "Undeclared value referenced"),
  DESC(ERROR_POINTER_ACCESS_ON_SCALAR,                   "Can't use '->' on a non-pointer type"),
  DESC(ERROR_UNKNOWN_MEMBER,                             "Type doesn't have requested member"),
  DESC(ERROR_MOD_NAME_NOT_IDENTIFIER,                    "Module name must be an identifier"),
  DESC(ERROR_ATTR_ARG_COUNT_MISMATCH,                    "Argument count mismatch for attribute"),
  DESC(ERROR_ATTR_ARG_TYPE_MISMATCH,                     "Argument type mismatch for attribute"),
  DESC(ERROR_INVALID_NUMBER_LITERAL,                     "Invalid number literal"),
  DESC(ERROR_TYPE_NOT_SUBSCRIPTABLE,                     "Type is not subscriptable"),
  DESC(ERROR_TYPE_NOT_VALID_SUBSCRIPT,                   "Type is not a valid subscript"),
  DESC(ERROR_TYPE_ALIAS_NAME_NOT_IDENTIFIER,             "Type alias name must be an identifier"),
  DESC(ERROR_TYPE_ALIAS_VALUE_NOT_TYPE,                  "Type alias value must be a type"),
  DESC(ERROR_INVALID_UNARY_AMP_RHS,                      "Invalid RHS for unary '&' operator"),
  DESC(ERROR_INVALID_UNARY_STAR_RHS,                     "Invalid RHS for unary '*' operator: value is not a pointer"),
  DESC(ERROR_UNKNOWN_UNARY_OP_OR_TYPE,                   "Unsupported unary expression operator or type"),
  DESC(ERROR_VARDECL_NO_VAL_AND_TYPE,                    "Value and type are missing from variable declaration"),
  DESC(ERROR_UNIMPLEMENTED,                              "Unimplemented functionality"),
  DESC(ERROR_UNKNOWN_TYPE,                               "Unknown type"),
  DESC(ERROR_MISSING_TYPE,                               "Missing type"),
  DESC(ERROR_UNEXPECTED_CHAR,                            "Unexpected character"),
  DESC(ERROR_NOT_A_TYPE,                                 "Expected a type"),
  DESC(ERROR_MODULE_NOT_FOUND,                           "Module not found"),
  DESC(ERROR_USE_MISSING_CLOSING_BRACE,                  "Missing '}' after inclusion symbol list"),
  DESC(ERROR_USE_WILDCARD_WITH_SYMBOLS,                  "Can't specify symbols to bring into current scope alongside with a wildcard"),
  DESC(ERROR_ALIAS_EXISTS,                               "Can't create an alias, as it already exists"),
  DESC(ERROR_TYPE_ARRAY_SIZE_NOT_NUMBER,                 "Array size must be an integer"),
  DESC(ERROR_TYPE_ARRAY_NO_CLOSING_BRACE,                "Missing ']' after array dimensions in type"),
  DESC(ERROR_ASM_EXPECTED_STRING,                        "Expected a string in asm! statement"),
  DESC(ERROR_INLINE_ATTR_INVALID_VALUE,                  "Invalid value for [inline] attribute (possible values: none, 'allways', 'never')"),
  DESC(ERROR_NO_SUCH_LOCAL_VARIABLE,                     "No such local variable"),
  DESC(ERROR_ENV_VAR_MISSING_DEFAULT,                    "Expected a string after '||' for default environment variable value"),
  DESC(ERROR_INIT_EXPECTED_NAMED_VALUE,                  "Expected a 'field: name' pair for struct initializer"),
  DESC(ERROR_BLOCK_MISSING_SEMICOLON,                    "Expected a ';' after a statement in block"),
  DESC(ERROR_INIT_MISSING_OPENING_BRACE,                 "Expected a '{' at the beginning of initializer expression"),
  DESC(ERROR_INIT_MISSING_CLOSING_BRACE,                 "Expected a '}' at the end of initializer expression"),
  DESC(ERROR_INIT_MISSING_CLOSING_SQUARE_BRACE,          "Expected a ']' after type in initializer expression"),
  DESC(ERROR_NOT_CONSTANT,                               "Provided value is not a constant expression"),
  DESC(ERROR_CANT_INFER_TYPE,                            "Can't infer type for expression"),
  DESC(ERROR_CONST_MISSING_KEYWORD,                      "Missing 'const' at the start of constant declaration"),
  DESC(ERROR_CONSTDECL_MISSING_SEMICOLON,                "Expected ';' after constant declaration"),
  DESC(ERROR_UNARY_RHS_NOT_CONSTANT,                     "Unary expression is not constant"),
  DESC(ERROR_NOT_ASM_IN_NAKED_FN,                        "Naked functions can only have asm! statements in body"),
  DESC(ERROR_TUPLE_MISSING_CLOSING_SQUARE_BRACE,         "Missing '[' after tuple declaration"),
  DESC(ERROR_TUPLE_MISSING_FIELDS,                       "Not all tuple members are initialized"),
  DESC(ERROR_DECOMPOSITION_EXPECTED_IDENTIFIER,          "Value decomposition expects an identifier as decomposed name"),
  DESC(ERROR_DECOMPOSITION_BAD_WILDCARD,                 "Decomposition wildcard '_' can only be placed at the end of the list"),
  DESC(ERROR_DECOMPOSITION_BAD_TYPE,                     "Cannot decompose value of unsupported type (supported types: tuple, struct, array)"),
  DESC(ERROR_DECOMPOSITION_MISSING_OPENING_SQUARE_BRACE, "Missing '[' at the beginning of value decomposition"),
  DESC(ERROR_DECOMPOSITION_MISSING_CLOSING_SQUARE_BRACE, "Missing ']' after variable list in value decomposition"),
  DESC(ERROR_DECOMPOSITION_MISSING_EQUALS,               "Missing '=' after variable list in value decomposition"),
  DESC(ERROR_INTERNAL_OUT_OF_BOUNDS,                     "(INTERNAL) Out of bounds access"),
  DESC(ERROR_UNINITIALIZABLE_TYPE,                       "Value of provided type cannot be initialized via an initializer expression"),
  DESC(ERROR_ENUM_MISSING_KEYWORD,                       "Missing 'enum' at the start of enum declaration"),
  DESC(ERROR_ENUM_MISSING_OPENING_BRACE,                 "Missing '{' at the start of enum declaration"),
  DESC(ERROR_ENUM_MISSING_CLOSING_BRACE,                 "Missing '}' after enum declaration"),
  DESC(ERROR_ENUM_NO_MEMBER,                             "No such enum member"),
  DESC(ERROR_ASSERTION,                                  "Compile-time assertion failed"),
  DESC(ERROR_USER_ERROR,                                 "Error triggered by error! macro"),
};

const std::unordered_map<WarningId, WarningDescription> WarningDescription::descs {
  DESC(WARNING_RESERVED,     "Reserved warning"),
  DESC(WARNING_USER_WARNING, "Warning triggered by warn! macro"),
};

static std::string generateHighlight(size_t line_ofs, size_t line_size, size_t len) {
  std::string res = ANSI_COLOR_FG_RED;

  for (size_t i = 0; i < line_ofs; ++i) {
    res += " ";
  }

  for (size_t i = 0; i < std::min(line_size, len); ++i) {
    res += "~";
  }

  return res + ANSI_TEXT_RESET;
}

const ErrorDescription& ErrorDescription::get(ErrorId id) {
  return descs.at(id);
}

const WarningDescription& WarningDescription::get(WarningId id) {
  return descs.at(id);
}

llvm::DILocation * SourceLocation::getDILocation(codegen::ModuleContext& ctx, llvm::DIScope * scope) const {
  if (!scope) {
    scope = ctx.currentDIScope();
  }

  return llvm::DILocation::get(scope->getContext(), line, column, scope);
}

SourceSpan SourceSpan::operator+(const SourceSpan& rhs) const {
  if (length == 0)     return rhs;
  if (rhs.length == 0) return *this;

  assertThrow(fileId == rhs.fileId,
    std::runtime_error("SourceSpan::operator+ can't combine spans originating from different files"));

  size_t start = std::min(offset, rhs.offset);
  size_t end = std::max(offset + length, rhs.offset + rhs.length);

  return {
    fileId,
    start,
    end - start
  };
}


SourceLocation SourceSpan::start() const {
  auto file = FileManager::get(fileId);

  if (!file) return {0, 0};

  auto line = file->lineByOffset(offset);
  auto line_info = file->lines[line];

  return {line, offset - line_info.offset};
}

SourceLocation SourceSpan::end() const {
  auto file      = FileManager::get(fileId);

  if (!file) return {0, 0};

  auto line      = file->lineByOffset(offset + length);
  auto line_info = file->lines[line];

  return {line, (offset + length) - line_info.offset};
}

SourceSpan& SourceSpan::operator+=(const SourceSpan& rhs) {
  *this = *this + rhs;
  return *this;
}

SourceSpan SourceSpan::pointToFirst() const {
  auto res = *this;

  res.length = 1;

  return res;
}

SourceSpan SourceSpan::pointToLast() const {
  auto res = *this;

  res.offset += res.length - 1;
  res.length = 1;

  return res;
}

SourceSpan SourceSpan::pointPastLast() const {
  auto res = *this;

  res.offset += res.length;
  res.length = 1;

  return res;
}

SourceSpan SourceSpan::builtin() {
  return {BuiltInFileId, 0, 0};
}

std::string SourceSpan::toString() const {
  if (fileId == BuiltInFileId) {
    return ANSI_COLOR_FG_YELLOW "      |" ANSI_TEXT_RESET "\n"
           ANSI_COLOR_FG_YELLOW "    0 |" ANSI_TEXT_RESET " " ANSI_TEXT_BOLD "<built-in/native>" ANSI_TEXT_RESET "\n"
           ANSI_COLOR_FG_YELLOW "      |" ANSI_TEXT_RESET "\n";
  }

  auto file = FileManager::get(fileId);
  if (!file) return "";

  auto line     = file->lineByOffset(offset);
  auto info     = file->lines[line];
  auto line_ofs = offset - info.offset;

  return std::format(
    ANSI_COLOR_FG_YELLOW  "     -->" ANSI_TEXT_RESET " {}:{}:{}\n"
    ANSI_COLOR_FG_YELLOW  "      |"  ANSI_TEXT_RESET "\n"
    ANSI_COLOR_FG_YELLOW " {:04} |"  ANSI_TEXT_RESET " " ANSI_TEXT_BOLD "{}" ANSI_TEXT_RESET "\n"
    ANSI_COLOR_FG_YELLOW  "      |"  ANSI_TEXT_RESET " {}\n",
    file->path, line, line_ofs, line,
    file->contents.substr(info.offset, info.length),
    generateHighlight(line_ofs, info.length, length)
  );
}

std::string Note::toString() const {
  return std::format(
      ANSI_COLOR_FG_GREEN "note:" ANSI_TEXT_RESET " " ANSI_TEXT_BOLD "{}" ANSI_TEXT_RESET "\n{}",
      message, span.toString()
    );
}

Warning::Warning(WarningId id, SourceSpan span) : id(id), span(span), message("") {}

std::string Warning::toString() const {
  std::string result = std::format(
    "warn[" ANSI_COLOR_FG_YELLOW "W{:04}" ANSI_TEXT_RESET "]: " ANSI_TEXT_BOLD "{}{}" ANSI_TEXT_RESET "\n{}",
    (size_t)id, WarningDescription::get(id).name,
    message.empty() ? "" : ": " + message,
    span.toString()
  );

  for (auto& note : notes) {
    result += note.toString();
  }

  return result;
}

void Warning::emit() const {
  auto str = toString();

  fprintf(stderr, "%s", str.c_str());
}

void Warning::emitFromNode(const ast::Node * node) const {
  auto warn = *this;

  if (node && node->hasAttribute("__xcc_macro_expanded_from")) {
    auto attr = node->getAttribute("__xcc_macro_expanded_from");
    auto name = attr.args[0]->as<ast::Identifier>();
    warn = warn.note(name->span, "Expanded from macro '{}'", name->name());
  }

  warn.emit();
}

Error::Error(ErrorId id, SourceSpan span) : id(id), span(span), message("") {}

std::string Error::toString() const {
  std::string result = std::format(
    "error[" ANSI_COLOR_FG_RED "E{:04}" ANSI_TEXT_RESET "]: " ANSI_TEXT_BOLD "{}{}" ANSI_TEXT_RESET "\n{}",
    (size_t)id, ErrorDescription::get(id).name,
    message.empty() ? "" : ": " + message,
    span.toString()
  );

  for (auto& note : notes) {
    result += note.toString();
  }

  return result;
}

void Error::raise() const {
  throw CompilationException(*this);
}

void Error::raiseFromNode(const ast::Node * node) const {
  auto err = *this;

  if (node && node->hasAttribute("__xcc_macro_expanded_from")) {
    auto attr = node->getAttribute("__xcc_macro_expanded_from");
    auto name = attr.args[0]->as<ast::Identifier>();
    err = err.note(name->span, "Expanded from macro '{}'", name->name());
  }

  err.raise();
}

#include "xcc/error.h"
#include "xcc/exceptions.h"
#include "xcc/util/fs.h"
#include "xcc/util/log.h"

using namespace xcc;

static auto logger = xcc::util::log::Logger("ERROR");

#define DESC(__err, __str) {__err, {__err, __str}}

const std::unordered_map<ErrorId, ErrorDescription> ErrorDescription::descs = {
  DESC(ERROR_RESERVED,                         "Reserved error"),
  DESC(ERROR_MISSING_FILE,                     "Can't open file"),
  DESC(ERROR_UNEXPECTED_EOF,                   "Unexpected End Of File"),
  DESC(ERROR_MISSING_CLOSING_QUOTE,            "Missing closing quote"),
  DESC(ERROR_MISSING_IDENTIFIER,               "Missing identifier"),
  DESC(ERROR_INVALID_MEMBER_ACCESS,            "Invalid member access"),
  DESC(ERROR_FN_TYPE_MISSING_OPENING_PAREN,    "Missing '(' after 'fn' in type declaration"),
  DESC(ERROR_FN_TYPE_MISSING_CLOSING_PAREN,    "Missing ')' after 'fn' arguments in type declaration"),
  DESC(ERROR_FN_TYPE_MISSING_ARROW,            "Missing '->' after ')' in 'fn' type declaration"),
  DESC(ERROR_FN_MISSING_KEYWORD,               "Missing 'fn' in function declaration"),
  DESC(ERROR_FN_MISSING_OPENING_PAREN,         "Missing '(' after 'fn' in function declaration"),
  DESC(ERROR_FN_MISSING_OPENING_PAREN,         "Missing ')' after arguments in function declaration"),
  DESC(ERROR_FN_MISSING_SEMICOLON,             "Missing ';' after function declaration"),
  DESC(ERROR_BLOCK_MISSING_OPENING_BRACE,      "Missing '{' at the start of block"),
  DESC(ERROR_BLOCK_MISSING_CLOSING_BRACE,      "Missing '}' at the end of block"),
  DESC(ERROR_VAR_MISSING_KEYWORD,              "Missing 'vat' at the start of variable declaration"),
  DESC(ERROR_STRUCT_MISSING_KEYWORD,           "Missing 'struct' at the start of struct declaration"),
  DESC(ERROR_STRUCT_MISSING_OPENING_BRACE,     "Missing '{' at the start of struct body declaration"),
  DESC(ERROR_STRUCT_MISSING_CLOSING_BRACE,     "Missing '{' at the end of struct body declaration"),
  DESC(ERROR_IF_MISSING_KEYWORD,               "Missing 'if' at the start of if statement"),
  DESC(ERROR_IF_MISSING_OPENING_PAREN,         "Missing '(' after 'if'"),
  DESC(ERROR_IF_MISSING_CLOSING_PAREN,         "Missing ')' after if condition"),
  DESC(ERROR_FOR_MISSING_KEYWORD,              "Missing 'for' at the start of for statement"),
  DESC(ERROR_FOR_MISSING_OPENING_PAREN,        "Missing '(' after 'for'"),
  DESC(ERROR_FOR_MISSING_CLOSING_PAREN,        "Missing ')' after 'for' conditions"),
  DESC(ERROR_FOR_MISSING_INIT_SEMICOLON,       "Missing ';' after init part of 'for'"),
  DESC(ERROR_FOR_MISSING_COND_SEMICOLON,       "Missing ';' after condition part of 'for'"),
  DESC(ERROR_WHILE_MISSING_KEYWORD,            "Missing 'while' at the start of while statement"),
  DESC(ERROR_WHILE_MISSING_OPENING_PAREN,      "Missing '(' after 'while'"),
  DESC(ERROR_WHILE_MISSING_CLOSING_PAREN,      "Missing ')' after 'while' condition"),
  DESC(ERROR_RETURN_MISSING_KEYWORD,           "Missing 'return' at the beginning of return statement"),
  DESC(ERROR_USE_MISSING_KEYWORD,              "Missing 'use' at the beginning of use statement"),
  DESC(ERROR_USE_MISSING_SEMICOLON,            "Missing ';' at the end of use statement"),
  DESC(ERROR_PATH_ATTR_ARG_MISMATCH,           "'path' attribute expects 1 argument"),
  DESC(ERROR_PATH_ATTR_ARG_BAD_TYPE,           "'path' attribute expect a string argument"),
  DESC(ERROR_MOD_MISSING_KEYWORD,              "Missing 'mod' at the beginning of module declaration"),
  DESC(ERROR_MOD_MISSING_SEMICOLON,            "Missing ';' after file-scoped module declaration"),
  DESC(ERROR_MOD_MISSING_CLOSING_BRACE,        "Missing '}' after module body"),
  DESC(ERROR_TYPE_MISSING_KEYWORD,             "Missing 'type' at the beginning of type declaration"),
  DESC(ERROR_TYPE_MISSING_EQUALS,              "Missing '=' after type name at the type declaration"),
  DESC(ERROR_TYPE_MISSING_SEMICOLON,           "Missing ';' at the end of type declaration"),
  DESC(ERROR_MACRO_MISSING_KEYWORD,            "Missing 'macro' at the beginning of macro declaration"),
  DESC(ERROR_MACRO_MISSING_OPENING_PAREN,      "Missing '(' after 'macro' at the macro declaration"),
  DESC(ERROR_MACRO_MISSING_OPENING_PAREN,      "Missing ')' after macro arguments at the macro declaration"),
  DESC(ERROR_INVALID_LHS_FOR_ASSIGNMENT,       "Invalid LHS for assignment (non-lvalue)"),
  DESC(ERROR_SUBSCRIPT_MISSING_CLOSING_BRACE,  "Missing closing ']' after '[' in subscript operator"),
  DESC(ERROR_DOLLAR_MISSING_IDENTIFIER,        "Expected identifier after '$'"),
  DESC(ERROR_NO_ENV_VARIABLE,                  "No such environment variable"),
  DESC(ERROR_LVALUE_UNEXPECTED_TOKEN,          "Unexpected token, expected identifier or self"),
  DESC(ERROR_MACRO_CALLEE_IS_NOT_ID,           "Macro callee can only be an identifier"),
  DESC(ERROR_MACRO_CALL_MISSING_OPENING_PAREN, "Expected '(' after macro name in macro call"),
  DESC(ERROR_MACRO_CALL_MISSING_CLOSING_PAREN, "Expected ')' after macro arguments in macro call"),
  DESC(ERROR_FN_CALL_MISSING_OPENING_PAREN,    "Expected '(' after function name in function call"),
  DESC(ERROR_FN_CALL_MISSING_CLOSING_PAREN,    "Expected ')' after function arguments in function call"),
  DESC(ERROR_ATTR_MISSING_OPENING_BRACKET,     "Expected '[' at the beginning of attribute list"),
  DESC(ERROR_ATTR_MISSING_CLOSING_BRACKET,     "Expected ']' after attribute list"),
  DESC(ERROR_ATTR_MISSING_CLOSING_PAREN,       "Expected ')' after attribute parameters"),
  DESC(ERROR_VARDECL_MISSING_SEMICOLON,        "Expected ';' variable declaration in global scope"),
  DESC(ERROR_TOP_LEVEL_UNEXPECTED_TOKEN,       "Unexpected token at top-level scope"),
  DESC(ERROR_LLVM_ERROR,                       "LLVM Error"),
  DESC(ERROR_UNKNOWN_GLOBAL_VARIABLE,          "Unknown global variable"),
  DESC(ERROR_UNKNOWN_FUNCTION,                 "Can't find function"),
  DESC(ERROR_UNKNOWN_SYMBOL,                   "Can't find symbol"),
  DESC(ERROR_INVALID_CAST,                     "Invalid cast"),
  DESC(ERROR_MACRO_CALL_ARG_COUNT_MISMATCH,    "Argument count mismatch for macro call"),
  DESC(ERROR_MACRO_CALL_ARG_TYPE_MISMATCH,     "Unexpected argument type for macro call"),
  DESC(ERROR_INVALID_TYPE,                     "Invalid type"),
  DESC(ERROR_INVALID_TOKEN_FOR_CONTEXT,        "Unexpected token in current context"),
  DESC(ERROR_UNKNOWN_MACRO,                    "Can't find macro"),
  DESC(ERROR_INVALID_ASSIGNMENT_OP,            "Invalid operation for assignment"),
  DESC(ERROR_UNKNOWN_VARIABLE,                 "Unknown variable"),
  DESC(ERROR_UNKNOWN_BIN_OP_OR_TYPE,           "Unsupported binary expression operator or type"),
  DESC(ERROR_INTERNAL_UNEXPECTED_NULL,         "(INTERNAL) Unexpected NULL value was received"),
  DESC(ERROR_FN_CALL_ARG_COUNT_MISMATCH,       "Argument count mismatch for function call"),
  DESC(ERROR_FN_CALL_ARG_TYPE_MISMATCH,        "Unexpected argument type for function call"),
  DESC(ERROR_INTERNAL_FAILURE,                 "(INTERNAL) Failed to perform action"),
  DESC(ERROR_EXPR_NOT_CALLABLE,                "Expression is no callable"),
  DESC(ERROR_UNKNOWN_METHOD,                   "Unknown method"),
  DESC(ERROR_INTERNAL_UNEXPECTED_NODE,         "Unexpected node"),
  DESC(ERROR_UNDECLARED_VALUE,                 "Undeclared value referenced"),
  DESC(ERROR_POINTER_ACCESS_ON_SCALAR,         "Can't use '->' on a non-pointer type"),
  DESC(ERROR_UNKNOWN_MEMBER,                   "Type doesn't have requested member"),
  DESC(ERROR_MOD_NAME_NOT_IDENTIFIER,          "Module name must be an identifier"),
  DESC(ERROR_ATTR_ARG_COUNT_MISMATCH,          "Argument count mismatch for attribute"),
  DESC(ERROR_ATTR_ARG_TYPE_MISMATCH,           "Argument type mismatch for attribute"),
  DESC(ERROR_INVALID_NUMBER_LITERAL,           "Invalid number literal"),
  DESC(ERROR_TYPE_NOT_SUBSCRIPTABLE,           "Type is not subscriptable"),
  DESC(ERROR_TYPE_NOT_VALID_SUBSCRIPT,         "Type is not a valid subscript"),
  DESC(ERROR_TYPE_ALIAS_NAME_NOT_IDENTIFIER,   "Type alias name must be an identifier"),
  DESC(ERROR_TYPE_ALIAS_VALUE_NOT_TYPE,        "Type alias value must be a type"),
  DESC(ERROR_INVALID_UNARY_AMP_RHS,            "Invalid RHS for unary '&' operator"),
  DESC(ERROR_INVALID_UNARY_STAR_RHS,           "Invalid RHS for unary '*' operator: value is not a pointer"),
  DESC(ERROR_UNKNOWN_UNARY_OP_OR_TYPE,         "Unsupported unary expression operator or type"),
  DESC(ERROR_VARDECL_NO_VAL_AND_TYPE,          "Value and type are missing from variable declaration"),
  DESC(ERROR_UNIMPLEMENTED,                    "Unimplemented functionality"),
  DESC(ERROR_UNKNOWN_TYPE,                     "Unknown type"),
};

static std::string generateHighlight(size_t line_ofs, size_t len) {
  std::string res;

  for (size_t i = 0; i < line_ofs; ++i) {
    res += " ";
  }

  for (size_t i = 0; i < len; ++i) {
    res += "~";
  }

  return res;
}

const ErrorDescription& ErrorDescription::get(ErrorId id) {
  return descs.at(id);
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
  return {BuildInFileId, 0, 0};
}

std::string SourceSpan::toString() const {
  if (fileId == BuildInFileId) {
    return "     | <built-in/native>\n";
  }

  auto file = FileManager::get(fileId);
  if (!file) return "";

  auto line     = file->lineByOffset(offset);
  auto info     = file->lines[line];
  auto line_ofs = offset - info.offset;

  return std::format(
    "     --> {}:{}:{}\n"
    "      |\n"
    " {:04} | {}\n"
    "      | {}\n",
    file->path, line, line_ofs, line,
    file->contents.substr(info.offset, info.length),
    generateHighlight(line_ofs, length)
  );
}

std::string Error::toString() const {
  std::string result = std::format(
    "error[E{:04}]: {}{}\n{}",
    (size_t)id, ErrorDescription::get(id).name,
    message.empty() ? "" : ": " + message,
    span.toString()
  );

  for (auto& note : notes) {
    result += std::format("note: {}\n{}", note.message, note.span.toString());
  }

  return result;
}

void Error::throwException() const {
  throw CompilationException(*this);
}

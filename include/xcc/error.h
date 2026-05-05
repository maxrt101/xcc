#pragma once

#include "xcc/util/filemng.h"

#include <llvm/Support/Error.h>

#include <format>
#include <string>
#include <vector>
#include <memory>

namespace xcc {

namespace ast {
class Node;
}

enum ErrorId {
  ERROR_RESERVED                         = 0,

  ERROR_MISSING_FILE                     = 1,
  ERROR_UNEXPECTED_EOF                   = 2,
  ERROR_MISSING_CLOSING_QUOTE            = 3,
  ERROR_MISSING_IDENTIFIER               = 4,
  ERROR_INVALID_MEMBER_ACCESS            = 5,
  ERROR_FN_TYPE_MISSING_OPENING_PAREN    = 6,
  ERROR_FN_TYPE_MISSING_CLOSING_PAREN    = 7,
  ERROR_FN_TYPE_MISSING_ARROW            = 8,
  ERROR_FN_MISSING_KEYWORD               = 9,
  ERROR_FN_MISSING_OPENING_PAREN         = 10,
  ERROR_FN_MISSING_CLOSING_PAREN         = 11,
  ERROR_FN_MISSING_SEMICOLON             = 12,
  ERROR_BLOCK_MISSING_OPENING_BRACE      = 13,
  ERROR_BLOCK_MISSING_CLOSING_BRACE      = 14,
  ERROR_VAR_MISSING_KEYWORD              = 15,
  ERROR_STRUCT_MISSING_KEYWORD           = 16,
  ERROR_STRUCT_MISSING_OPENING_BRACE     = 16,
  ERROR_STRUCT_MISSING_CLOSING_BRACE     = 17,
  ERROR_IF_MISSING_KEYWORD               = 18,
  ERROR_IF_MISSING_OPENING_PAREN         = 19,
  ERROR_IF_MISSING_CLOSING_PAREN         = 20,
  ERROR_FOR_MISSING_KEYWORD              = 21,
  ERROR_FOR_MISSING_OPENING_PAREN        = 22,
  ERROR_FOR_MISSING_CLOSING_PAREN        = 23,
  ERROR_FOR_MISSING_INIT_SEMICOLON       = 24,
  ERROR_FOR_MISSING_COND_SEMICOLON       = 25,
  ERROR_WHILE_MISSING_KEYWORD            = 26,
  ERROR_WHILE_MISSING_OPENING_PAREN      = 27,
  ERROR_WHILE_MISSING_CLOSING_PAREN      = 28,
  ERROR_RETURN_MISSING_KEYWORD           = 29,
  ERROR_USE_MISSING_KEYWORD              = 30,
  ERROR_USE_MISSING_SEMICOLON            = 31,
  ERROR_PATH_ATTR_ARG_MISMATCH           = 32,
  ERROR_PATH_ATTR_ARG_BAD_TYPE           = 33,
  ERROR_MOD_MISSING_KEYWORD              = 34,
  ERROR_MOD_MISSING_SEMICOLON            = 35,
  ERROR_MOD_MISSING_CLOSING_BRACE        = 36,
  ERROR_TYPE_MISSING_KEYWORD             = 37,
  ERROR_TYPE_MISSING_EQUALS              = 38,
  ERROR_TYPE_MISSING_SEMICOLON           = 39,
  ERROR_MACRO_MISSING_KEYWORD            = 40,
  ERROR_MACRO_MISSING_OPENING_PAREN      = 41,
  ERROR_MACRO_MISSING_CLOSING_PAREN      = 42,
  ERROR_INVALID_LHS_FOR_ASSIGNMENT       = 43,
  ERROR_SUBSCRIPT_MISSING_CLOSING_BRACE  = 44,
  ERROR_DOLLAR_MISSING_IDENTIFIER        = 45,
  ERROR_NO_ENV_VARIABLE                  = 46,
  ERROR_EXPR_MISSING_CLOSING_PAREN       = 47,
  ERROR_LVALUE_UNEXPECTED_TOKEN          = 48,
  ERROR_MACRO_CALLEE_IS_NOT_ID           = 49,
  ERROR_MACRO_CALL_MISSING_OPENING_PAREN = 50,
  ERROR_MACRO_CALL_MISSING_CLOSING_PAREN = 51,
  ERROR_FN_CALL_MISSING_OPENING_PAREN    = 52,
  ERROR_FN_CALL_MISSING_CLOSING_PAREN    = 53,
  ERROR_ATTR_MISSING_OPENING_BRACKET     = 54,
  ERROR_ATTR_MISSING_CLOSING_BRACKET     = 55,
  ERROR_ATTR_MISSING_CLOSING_PAREN       = 56,
  ERROR_VARDECL_MISSING_SEMICOLON        = 57,
  ERROR_TOP_LEVEL_UNEXPECTED_TOKEN       = 58,
  ERROR_LLVM_ERROR                       = 59,
  ERROR_UNKNOWN_GLOBAL_VARIABLE          = 60,
  ERROR_UNKNOWN_FUNCTION                 = 61,
  ERROR_UNKNOWN_SYMBOL                   = 62,
  ERROR_INVALID_CAST                     = 63,
  ERROR_MACRO_CALL_ARG_COUNT_MISMATCH    = 64,
  ERROR_MACRO_CALL_ARG_TYPE_MISMATCH     = 65,
  ERROR_INVALID_TYPE                     = 66,
  ERROR_INVALID_TOKEN_FOR_CONTEXT        = 67,
  ERROR_UNKNOWN_MACRO                    = 68,
  ERROR_INVALID_ASSIGNMENT_OP            = 69,
  ERROR_UNKNOWN_VARIABLE                 = 70,
  ERROR_UNKNOWN_BIN_OP_OR_TYPE           = 71,
  ERROR_INTERNAL_UNEXPECTED_NULL         = 72,
  ERROR_FN_CALL_ARG_COUNT_MISMATCH       = 73,
  ERROR_FN_CALL_ARG_TYPE_MISMATCH        = 74,
  ERROR_INTERNAL_FAILURE                 = 75,
  ERROR_EXPR_NOT_CALLABLE                = 76,
  ERROR_UNKNOWN_METHOD                   = 77,
  ERROR_INTERNAL_UNEXPECTED_NODE         = 78,
  ERROR_UNDECLARED_VALUE                 = 79,
  ERROR_POINTER_ACCESS_ON_SCALAR         = 80,
  ERROR_UNKNOWN_MEMBER                   = 81,
  ERROR_MOD_NAME_NOT_IDENTIFIER          = 82,
  ERROR_ATTR_ARG_COUNT_MISMATCH          = 83,
  ERROR_ATTR_ARG_TYPE_MISMATCH           = 84,
  ERROR_INVALID_NUMBER_LITERAL           = 85,
  ERROR_TYPE_NOT_SUBSCRIPTABLE           = 86,
  ERROR_TYPE_NOT_VALID_SUBSCRIPT         = 87,
  ERROR_TYPE_ALIAS_NAME_NOT_IDENTIFIER   = 89,
  ERROR_TYPE_ALIAS_VALUE_NOT_TYPE        = 90,
  ERROR_INVALID_UNARY_AMP_RHS            = 91,
  ERROR_INVALID_UNARY_STAR_RHS           = 92,
  ERROR_UNKNOWN_UNARY_OP_OR_TYPE         = 93,
  ERROR_VARDECL_NO_VAL_AND_TYPE          = 94,
  ERROR_UNIMPLEMENTED                    = 95,
  ERROR_UNKNOWN_TYPE                     = 96,
  ERROR_MISSING_TYPE                     = 97,

  ERROR_MAX,
};

struct ErrorDescription {
  ErrorId     id;
  std::string name;
  // std::string_view fmt; // TODO: Use format?

  static const ErrorDescription& get(ErrorId id);

private:
  static const std::unordered_map<ErrorId, ErrorDescription> descs;
};

struct SourceSpan {
  FileId fileId = 0;
  size_t offset = 0;
  size_t length = 0;

  SourceSpan operator+(const SourceSpan& rhs) const;
  SourceSpan& operator+=(const SourceSpan& rhs);

  SourceSpan pointToFirst() const;
  SourceSpan pointToLast() const;
  SourceSpan pointPastLast() const;

  [[nodiscard]] std::string toString() const;

  static SourceSpan builtin();
};

struct Note {
  SourceSpan  span;
  std::string message;

  template <typename... Args>
  Note(SourceSpan span, std::string_view fmt, Args&&... args)
    : span(span), message(std::vformat(fmt, std::make_format_args(args...))) {}
};

struct Error {
  ErrorId     id;
  SourceSpan  span;
  std::string message;

  std::vector<Note> notes;

  template <typename... Args>
  Error(ErrorId id, SourceSpan span, std::string_view fmt, Args&&... args)
    : id(id), span(span), message(std::vformat(fmt, std::make_format_args(args...))) {}

  template <typename... Args>
  Error& note(SourceSpan span, std::string_view fmt, Args&&... args) {
    notes.push_back(Note(span, fmt, std::forward<Args>(args)...));
    return *this;
  }

  [[nodiscard]] std::string toString() const;

  [[noreturn]] void raise() const;
  [[noreturn]] void raiseFromNode(ast::Node * node) const;
};

template <typename E>
void assertRaise(bool expr, const E& err) {
  if (!expr) {
    err.raise();
  }
}

template <typename T, typename E>
T raiseIfNull(T expr, const E& err) {
  if (!expr) {
    err.raise();
  }

  return expr;
}

static void checkLLVMError(llvm::Error&& err) {
  if (bool(err)) {
    std::string str;
    llvm::raw_string_ostream output(str);
    output << err;

    Error(ERROR_LLVM_ERROR, {}, str).raise();
  }
}

}

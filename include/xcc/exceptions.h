#pragma once

#include <exception>
#include <string>

#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>

namespace xcc {

/**
 * User by Lexer to signal an error
 */
class LexerException : public std::exception {
  std::string msg;

public:
  explicit inline LexerException(const std::string& msg) : msg("LexerException: " + msg) {}

  inline LexerException(size_t line, const std::string& msg) {
    this->msg = "LexerException at line " + std::to_string(line) + ": " + msg;
  }

  [[nodiscard]] inline const char * what() const noexcept override {
    return msg.c_str();
  }
};

/**
 * User by Parser to signal an error
 */
class ParserException : public std::exception {
  std::string msg;

public:
  explicit inline ParserException(const std::string& msg) : msg("ParserException: " + msg) {}

  inline ParserException(size_t line, const std::string& msg) {
    this->msg = "ParserException at line " + std::to_string(line) + ": " + msg;
  }

  [[nodiscard]] inline const char * what() const noexcept override {
    return msg.c_str();
  }
};

/**
 * User by Node::generate* to signal an error
 */
class CodegenException : public std::exception {
  std::string msg;

public:
  explicit inline CodegenException(const std::string& msg) : msg("CodegenException: " + msg) {}

  inline CodegenException(size_t line, const std::string& msg) {
    this->msg = "CodegenException at line " + std::to_string(line) + ": " + msg;
  }

  inline CodegenException(llvm::Error&& err) {
    std::string str;
    llvm::raw_string_ostream output(str);
    this->msg = "LLVM Error:" + str;
  }

  static inline void throwIfError(llvm::Error&& err) {
    if (bool(err)) {
      throw CodegenException(std::move(err));
    }
  }

  [[nodiscard]] inline const char * what() const noexcept override {
    return msg.c_str();
  }
};

/**
 * Helper function to throw a specific exception if expr is false
 *
 * @tparam E Exception type. Usually inferred by the compiler from `ex`
 * @param expr true - OK, false - will throw
 * @param ex Exception to throw if check fails
 */
template <typename E>
inline void assertThrow(bool expr, const E& ex) {
  if (!expr) {
    throw ex;
  }
}

/**
 * Helper function to throw a specific exception if expr is NULL
 *
 * @tparam T Type of expr. Usually inferred by the compiler from `expr`
 * @tparam E Exception type. Usually inferred by the compiler from `ex`
 * @param expr Value to check for being NULL
 * @param ex Exception to throw if check fails
 * @return expr, if it's not NULL
 */
template <typename T, typename E>
inline T throwIfNull(T expr, const E& ex) {
  if (!expr) {
    throw ex;
  }

  return expr;
}

} /* namespace xcc */

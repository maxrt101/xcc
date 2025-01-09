#pragma once

#include <exception>
#include <string>

#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>

namespace xcc {

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

template <typename E>
inline void assertThrow(bool expr, const E& ex) {
  if (!expr) {
    throw ex;
  }
}

template <typename T, typename E>
inline T throwIfNull(T expr, const E& ex) {
  if (!expr) {
    throw ex;
  }

  return expr;
}

} /* namespace xcc */

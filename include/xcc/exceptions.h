#pragma once

#include <exception>
#include <string>

#include "xcc/error.h"

namespace xcc {

class CompilationException : public std::exception {
public:
  Error       error;
  std::string msg;

public:
  explicit CompilationException(const Error& err)
    : error(err), msg(err.toString()) {}

  [[nodiscard]] const char * what() const noexcept override {
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
void assertThrow(bool expr, const E& ex) {
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
T throwIfNull(T expr, const E& ex) {
  if (!expr) {
    throw ex;
  }

  return expr;
}

} /* namespace xcc */

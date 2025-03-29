#pragma once

#include "xcc/meta/type.h"
#include "xcc/exceptions.h"
#include "xcc/codegen.h"
#include "xcc/lexer.h"
#include <llvm/IR/IRBuilder.h>
#include <vector>

/**
 * Helper macro to expand value encased in `()`
 *
 * @warning Internal
 */
#define XCC_BINOP_EXPAND(...) __VA_ARGS__

/**
 * Base class name, for which instruction generators are members
 *
 * @warning Internal
 */
#define XCC_BINOP_HANDLER_BASE_CLASS llvm::IRBuilderBase

/**
 * Returns type of instruction generator
 *
 * @warning Internal
 */
#define XCC_BINOP_HANDLER_RETURN llvm::Value *

/**
 * Arguments of a base instruction generators
 *
 * @warning Internal
 */
#define XCC_BINOP_HANDLER_BASE_ARGS llvm::Value*, llvm::Value*, const llvm::Twine&

/**
 * Implementation of XCC_BINOP_HANDLER_TYPE
 *
 * @warning Internal
 */
#define XCC_BINOP_HANDLER_TYPE_IMPL(...) \
  XCC_BINOP_HANDLER_RETURN (XCC_BINOP_HANDLER_BASE_CLASS::*)(XCC_BINOP_HANDLER_BASE_ARGS, ## __VA_ARGS__)

/**
 * Generate instruction generator function type, with __VA_ARGS__ being additional arguments
 *
 * @warning Internal
 */
#define XCC_BINOP_HANDLER_TYPE(...) \
  XCC_BINOP_HANDLER_TYPE_IMPL(__VA_ARGS__)

/**
 * Creates a BinaryOperation
 *
 * Example:
 * @code{.c}
 *   binop::List ops = {
 *     XCC_BINOP(TOKEN_PLUS, INTEGER, CreateAdd,  (bool, bool)),
 *     XCC_BINOP(TOKEN_PLUS, FLOAT,   CreateFAdd, ())
 *   };
 * @endcode
 *
 * @param __op    Operation (operator) - TokenType
 * @param __cond  Bitmask of BinaryOperationConditions
 * @param __fn    Function name to generate instruction from lhs & rhs. Must be
 *                a member of XCC_BINOP_HANDLER_BASE_CLASS
 * @param __twine Twine (temporary value name)
 * @param ...     Additional arguments that the function __fn expects
 *                (besides lhs, rhs & twine)
 */
#define XCC_BINOP(__op, __cond, __fn, __twine, ...)                                                           \
  {                                                                                                           \
    {__op, __cond},                                                                                           \
    binop::Handler::create((XCC_BINOP_HANDLER_TYPE(XCC_BINOP_EXPAND __VA_ARGS__)) &llvm::IRBuilder<>::__fn),  \
    __twine                                                                                                   \
  }

/**
 * Default values to be passed to handlers for unaccounted additional arguments
 *
 * @warning Internal
 */
#define XCC_BINOP_VARGS_DEFAULT_VALUES 0, 0

namespace xcc::binop {

/**
 * Represents condition, a type must meet for handler to be called
 */
namespace Conditions {
  constexpr uint8_t NONE        = 0;
  constexpr uint8_t INTEGER     = 1 << 0;
  constexpr uint8_t FLOAT       = 1 << 1;
  constexpr uint8_t SIGNED      = 1 << 2;
  constexpr uint8_t UNSIGNED    = 1 << 3;
}

/**
 * Handler for binary operation
 */
struct Handler {
  /**
   * Base instruction generator type
   *
   * ... is appended to account for additional arguments
   * This is a *very* sketchy workaround for sake of generic handlers
   */
  using Base = XCC_BINOP_HANDLER_TYPE(...);

public:
  Base handler;

public:
  /**
   * Call underlying handler (instruction generator)
   *
   * @param ctx Module Context
   * @param lhs LeftHandSide of binary operation
   * @param rhs RightHandSide of binary operation
   * @param twine Temporary result name
   * @return Operation result
   */
  llvm::Value * operator()(
    codegen::ModuleContext& ctx,
    llvm::Value * lhs,
    llvm::Value * rhs,
    const std::string& twine
  ) const;

  /**
   * Creates generic binary operation Handler from specific handler
   *
   * @tparam T Handler member function type
   * @param handler Function
   * @return New Handler instance
   */
  template <typename T>
  static Handler create(T handler) {
    return Handler {(Base) handler};
  }
};

/**
 * Metadata of binary operation
 *
 * Needed to decide which handler to call, based on operator (op) & condition (cond)
 * Condition is a bitmask of BinaryOperationConditions
 */
struct Meta {
  TokenType op;
  uint8_t cond;

public:
  /**
   * Check if `rhs` meets conditions (op & cond)
   *
   * Has a special way of determining if condition is met, e.g. INTEGER gets special treatment
   * because if binop doesn't specify neither SIGNED nor UNSIGNED check should pass regardless
   * of signess in `rhs`
   *
   * @param rhs BinaryOperationMeta to check condition of
   */
  bool check(const Meta& rhs) const;

  /**
   * Creates a human-readable representation of BinaryOperationMeta
   */
  std::string toString() const;

  /**
   * Creates BinaryOperationMeta from operator and meta::Type
   *
   * @param op   Operator (represented by a token type)
   * @param type Type metadata, from which cond is formed
   */
  static Meta fromType(TokenType op, std::shared_ptr<meta::Type> type);

};

/**
 * Binary Operation - meta, handler & twine
 */
struct Context {
  Meta meta;
  Handler handler;
  std::string twine;
};

/**
 * Shortcut for a list of binary operations
 */
using List = std::vector<Context>;

/**
 * Looks for binary operation in a list by meta
 *
 * @param binops  Binary operation list
 * @param meta    Binop metadata that acts as a 'key' to look by
 * @returns nullptr If not found
 */
const Context * findBinaryOperation(const List& binops, const Meta& meta);

}

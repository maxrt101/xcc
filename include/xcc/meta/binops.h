#pragma once

#include "xcc/meta/type.h"
#include "xcc/lexer.h"
#include <functional>
#include <vector>

/**
 * Creates a BinaryOperation
 *
 * Example:
 * @code{.c}
 *   BinaryOperations ops = {
 *     XCC_BINOP(TOKEN_PLUS, INTEGER, ctx.ir_builder->CreateAdd(lhs, rhs)),
 *     XCC_BINOP(TOKEN_PLUS, FLOAT,   ctx.ir_builder->CreateFAdd(lhs, rhs))
 *   };
 * @endcode
 *
 * @param __op    Operation (operator) - TokenType
 * @param __cond  Bitmask of BinaryOperationConditions
 * @param ...     Code to generate llvm::Value * from lhs & rhs
 */
#define XCC_BINOP(__op, __cond, ...)                                          \
  {                                                                           \
    {__op, __cond},                                                           \
    [](codegen::ModuleContext& ctx,                                           \
       llvm::Value * lhs,                                                     \
       llvm::Value * rhs                                                      \
      ) -> llvm::Value * {                                                    \
        return __VA_ARGS__;                                                   \
      }                                                                       \
  }

namespace xcc {

/**
 * Represents condition, a type must meet for handler to be called
 */
namespace BinaryOperationConditions {
  constexpr uint8_t NONE        = 0;
  constexpr uint8_t INTEGER     = 1 << 0;
  constexpr uint8_t FLOAT       = 1 << 1;
  constexpr uint8_t SIGNED      = 1 << 2;
  constexpr uint8_t UNSIGNED    = 1 << 3;
}

/**
 * Handler for binary operation
 */
using BinaryOperationHandler = std::function<llvm::Value*(codegen::ModuleContext&, llvm::Value *, llvm::Value *)>;

/**
 * Metadata of binary operation
 *
 * Needed to decide which handler to call, based on operator (op) & condition (cond)
 * Condition is a bitmask of BinaryOperationConditions
 */
struct BinaryOperationMeta {
  TokenType op;
  uint8_t cond;

public:
  /**
   * Check if `rhs` meets conditions (op & cond)
   *
   * Has a special way of determining if condition is met, e.g. INTEGER gets special treatment
   * because if binop doesn't specify neither SIGNED nor UNSIGNED check should pass regardless
   * of sign in `rhs`
   *
   * @param rhs BinaryOperationMeta to check condtion of
   */
  bool check(const BinaryOperationMeta& rhs) const;

  /**
   * Creates a human-readable representation of BinaryOperationMeta
   */
  std::string toString() const;

  /**
   * Creates BinaryOperationMeta from operator and meta::Type
   *
   * @param op   Operator (represented by a token type)
   * @param type Type metadata, from which cond is fotmed
   */
  static BinaryOperationMeta fromType(TokenType op, std::shared_ptr<meta::Type> type);

};

/**
 * Binary Operation - meta + handler
 */
struct BinaryOperation {
  BinaryOperationMeta meta;
  BinaryOperationHandler handler;
};

/**
 * Shortcut for a list of binary operations
 */
using BinaryOperations = std::vector<BinaryOperation>;

/**
 * Looks for binary operation in a list by meta
 *
 * @param binops  Binary operation list
 * @param meta    Binop metadata that acts as a 'key' to look by
 * @returns nullptr If not found
 */
BinaryOperation * findBinaryOperation(BinaryOperations& binops, BinaryOperationMeta& meta);

}

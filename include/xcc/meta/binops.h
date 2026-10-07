#pragma once

#include "xcc/meta/type.h"
#include "xcc/codegen.h"
#include "xcc/lexer.h"
#include <functional>
#include <vector>

/**
 * Creates a BinaryOperation
 */
#define XCC_BINOP(__op, __cond, __fn, __twine)                                 \
  {                                                                            \
    {__op, __cond},                                                            \
    binop::Handler{[](codegen::ModuleContext& ctx, llvm::Value* l,             \
                      llvm::Value* r, const std::string& t) -> llvm::Value* {  \
      return ctx.ir_builder->__fn(l, r, t);                                    \
    }},                                                                        \
    __twine                                                                    \
  }

namespace xcc::binop {

namespace Conditions {
  constexpr uint8_t NONE        = 0;
  constexpr uint8_t INTEGER     = 1 << 0;
  constexpr uint8_t FLOAT       = 1 << 1;
  constexpr uint8_t SIGNED      = 1 << 2;
  constexpr uint8_t UNSIGNED    = 1 << 3;
}

/**
 * Handler for binary operation using safe std::function
 */
struct Handler {
  std::function<llvm::Value*(codegen::ModuleContext&, llvm::Value*, llvm::Value*, const std::string&)> fn;

  llvm::Value * operator()(
    codegen::ModuleContext& ctx,
    llvm::Value * lhs,
    llvm::Value * rhs,
    const std::string& twine
  ) const {
    return fn(ctx, lhs, rhs, twine);
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

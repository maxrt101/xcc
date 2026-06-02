#pragma once

#include <llvm/IR/Instructions.h>
#include "xcc/meta/type.h"

namespace xcc::meta {

/**
 * State of a variable
 * TODO: MAYBE_MOVED
 */
enum class Liveness {
  UNINITIALIZED = 0,
  INITIALIZED,
  MOVED
};

/**
 * Value - Type pair
 */
class TypedValue {
public:
  SourceSpan            span;
  std::shared_ptr<Type> type;
  llvm::AllocaInst *    value;

  // Track current state and last location of a move
  Liveness              state = Liveness::INITIALIZED;
  SourceSpan            moved_at;

public:
  TypedValue();
  TypedValue(SourceSpan span, std::shared_ptr<Type> type, llvm::AllocaInst * value);
  ~TypedValue() = default;

  static std::shared_ptr<TypedValue> create(codegen::ModuleContext& ctx, llvm::Function * fn, SourceSpan span, std::shared_ptr<Type> type, const std::string& name);
};

}


#pragma once

#include <llvm/IR/Instructions.h>
#include "xcc/meta/type.h"

namespace xcc::meta {

/**
 * Value - Type pair
 */
class TypedValue {
public:
  SourceSpan            span;
  std::shared_ptr<Type> type;
  llvm::AllocaInst *    value;

public:
  TypedValue();
  TypedValue(SourceSpan span, std::shared_ptr<Type> type, llvm::AllocaInst * value);
  ~TypedValue() = default;

  static std::shared_ptr<TypedValue> create(codegen::ModuleContext& ctx, llvm::Function* fn, SourceSpan span, std::shared_ptr<Type> type, const std::string& name);
};

}


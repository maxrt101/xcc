#pragma once

#include <llvm/IR/Instructions.h>
#include "xcc/meta/type.h"

namespace xcc::meta {

class TypedValue {
public:
  std::shared_ptr<xcc::meta::Type> type;
  llvm::AllocaInst * value;

public:
  TypedValue();
  TypedValue(std::shared_ptr<xcc::meta::Type> type, llvm::AllocaInst * value);
  ~TypedValue() = default;

  static std::shared_ptr<TypedValue> create(codegen::ModuleContext& ctx, llvm::Function* fn, std::shared_ptr<xcc::meta::Type> type, const std::string& name);
};

}


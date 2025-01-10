#pragma once

#include <llvm/IR/Type.h>
#include <llvm/ExecutionEngine/Orc/Shared/ExecutorSymbolDef.h>

#include "xcc/meta/type.h"

namespace xcc::util {

class GenericValueContainer {
public:
  enum Tag {
    VOID,
    SIGNED_INTEGER,
    UNSIGNED_INTEGER,
    FLOATING,
  } tag;

  union Value {
    uint64_t unsigned_integer;
    int64_t signed_integer;
    double floating;
  } value;

public:
  explicit GenericValueContainer(Tag tag = VOID);
  GenericValueContainer(Tag tag, Value value);

  static GenericValueContainer signedInt(int64_t value);
  static GenericValueContainer unsignedInt(uint64_t value);
  static GenericValueContainer floating(double value);
};

inline bool isFloatOrDouble(llvm::Type * type) {
  return type->isFloatTy() || type->isDoubleTy();
}

inline bool isInteger(llvm::Type * type) {
  return type->isIntegerTy();
}

inline bool isPointer(llvm::Type * type) {
  return type->isPointerTy();
}

inline bool isArray(llvm::Type * type) {
  return type->isArrayTy();
}

inline bool compareTypes(llvm::Type * lhs, llvm::Type * rhs) {
  return lhs == rhs;
}

template<typename T>
inline T call(llvm::orc::ExecutorSymbolDef& symbol) {
  T (*sym)() = symbol.getAddress().toPtr<T (*)()>();
  return sym();
}

GenericValueContainer call(std::shared_ptr<xcc::meta::Type> type, llvm::orc::ExecutorSymbolDef symbol);

}
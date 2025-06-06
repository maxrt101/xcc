#pragma once

#include <llvm/IR/Type.h>
#include <llvm/ExecutionEngine/Orc/Shared/ExecutorSymbolDef.h>

#include "xcc/meta/type.h"

namespace xcc::util {

/**
 * Generic container for function result, when function type is not known beforehand
 *
 * Basically a tagged union
 */
class GenericValueContainer {
public:
  /**
   * Type tag
   */
  enum Tag {
    VOID,
    SIGNED_INTEGER,
    UNSIGNED_INTEGER,
    FLOATING,
  } tag;

  /**
   * Value, should be accessed based on tag
   */
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

/**
 * Helper class for collecting logs from LLVM dump() functions
 */
class RawStreamCollector {
private:
  llvm::raw_string_ostream raw_stream;
  std::string buffer;

public:
  RawStreamCollector() : raw_stream(buffer) {}
  ~RawStreamCollector() = default;

  llvm::raw_ostream * stream() {
    return &raw_stream;
  }

  std::string& string() {
    return buffer;
  }
};

/**
 * Checks if LLVM Type is floating or double
 *
 * @param type LLVM Type
 */
inline bool isFloatOrDouble(llvm::Type * type) {
  return type->isFloatTy() || type->isDoubleTy();
}

/**
 * Checks if LLVM Type is integer
 *
 * @param type LLVM Type
 */
inline bool isInteger(llvm::Type * type) {
  return type->isIntegerTy();
}

/**
 * Checks if LLVM Type is pointer
 *
 * @param type LLVM Type
 */
inline bool isPointer(llvm::Type * type) {
  return type->isPointerTy();
}

/**
 * Checks if LLVM Type is an array
 *
 * @param type LLVM Type
 */
inline bool isArray(llvm::Type * type) {
  return type->isArrayTy();
}

/**
 * Compares LLVM types
 *
 * Since LLVM has a type pool, and each unique type has single instance, can just compare pointers
 */
inline bool compareTypes(llvm::Type * lhs, llvm::Type * rhs) {
  return lhs == rhs;
}

/**
 * Call a function from resolved symbol
 *
 * @tparam T Function return type
 * @param symbol Resolved symbol
 * @return Function return value
 */
template<typename T>
T call(llvm::orc::ExecutorSymbolDef& symbol) {
  T (*sym)() = symbol.getAddress().toPtr<T (*)()>();
  return sym();
}

/**
 * Call a function, and receive value container, from which return value can be retrieved
 *
 * @param type Function return type metadata
 * @param symbol Resolved symbol
 * @return generic value container
 */
GenericValueContainer call(std::shared_ptr<xcc::meta::Type> type, llvm::orc::ExecutorSymbolDef symbol);

}
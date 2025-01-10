#pragma once

#include <llvm/IR/DerivedTypes.h>

namespace xcc::codegen {
class ModuleContext;
}

namespace xcc::meta {

enum class TypeTag {
  VOID = 0,
  U8,
  I8,
  U16,
  I16,
  U32,
  I32,
  U64,
  I64,
  F32,
  F64,
  PTR
};

class Type {
private:
  TypeTag tag;
  std::shared_ptr<Type> pointedType;

public:
  explicit Type(TypeTag tag);
  ~Type() = default;

  bool operator==(Type& rhs);
  bool operator!=(Type& rhs);

  [[nodiscard]] TypeTag getTag() const;
  [[nodiscard]] std::shared_ptr<Type> getPointedType() const;
  [[nodiscard]] llvm::Type * getLLVMType(codegen::ModuleContext& ctx) const;

  [[nodiscard]] std::string toString() const;

  [[nodiscard]] bool isVoid() const;
  [[nodiscard]] bool isSigned() const;
  [[nodiscard]] bool isInteger() const;
  [[nodiscard]] bool isFloat() const;
  [[nodiscard]] bool isPointer() const;

  int getNumberBitWidth() const;

  llvm::Value * getDefault(codegen::ModuleContext& ctx) const;

  inline bool is(TypeTag expected) const {
    return tag == expected;
  }

  template <typename ...Types>
  inline bool isAnyOf(Types... expected) const {
    return ((this->tag == expected) || ...);
  }

  static std::shared_ptr<Type> create(TypeTag tag);
  static std::shared_ptr<Type> fromTypeName(const std::string& name);

  static std::shared_ptr<Type> createVoid();
  static std::shared_ptr<Type> createI8();
  static std::shared_ptr<Type> createI16();
  static std::shared_ptr<Type> createI32();
  static std::shared_ptr<Type> createI64();
  static std::shared_ptr<Type> createU8();
  static std::shared_ptr<Type> createU16();
  static std::shared_ptr<Type> createU32();
  static std::shared_ptr<Type> createU64();
  static std::shared_ptr<Type> createF32();
  static std::shared_ptr<Type> createF64();
  static std::shared_ptr<Type> createSigned(int bits);
  static std::shared_ptr<Type> createUnsigned(int bits);
  static std::shared_ptr<Type> createFloating(int bits);
  static std::shared_ptr<Type> createPointer(std::shared_ptr<Type> pointedType);

  static std::shared_ptr<Type> alignTypes(std::shared_ptr<Type> lhs, std::shared_ptr<Type> rhs);
};

};

#pragma once

#include <llvm/IR/DerivedTypes.h>

namespace xcc::codegen {
class ModuleContext;
}

namespace xcc::meta {

/**
 * Type Tag for meta::Type
 *
 * Basically a type of type
 */
enum class TypeTag {
  VOID = 0, /** void type - empty */
  U8,       /** unsigned 8-bit integer */
  I8,       /** signed 8-bit integer */
  U16,      /** unsigned 16-bit integer */
  I16,      /** signed 16-bit integer */
  U32,      /** unsigned 32-bit integer */
  I32,      /** signed 32-bit integer */
  U64,      /** unsigned 64-bit integer */
  I64,      /** signed 64-bit integer */
  F32,      /** 32-bit float point (equivalent to `float` type in C) */
  F64,      /** 64-bit float point (equivalent to `double` type in C) */
  PTR,      /** Generic/Opaque pointer type */
  STRUCT,   /** Type tag for user-defined types */
};

class Type;

/**
 * Shortcut for struct members list
 *
 * Is a list of pair of member names and types
 */
using StructMembers = std::vector<std::pair<std::string, std::shared_ptr<Type>>>;

/**
 * Type metadata
 */
class Type {
private:
  TypeTag tag;

  /** For TypeTag::PTR */
  std::shared_ptr<Type> pointedType;

  /** For TypeTag::STRUCT */
  StructMembers members;

  /** Global static storage for all user-defined types */
  static std::unordered_map<std::string, std::shared_ptr<Type>> customTypes;

public:
  explicit Type(TypeTag tag);
  ~Type() = default;

  bool operator==(Type& rhs);
  bool operator!=(Type& rhs);

  /**
   * Return type tag
   */
  [[nodiscard]] TypeTag getTag() const;

  /**
   * If type is a pointer - return pointed type
   */
  [[nodiscard]] std::shared_ptr<Type> getPointedType() const;

  /**
   * If type is a struct - check if it has member with name `name`
   */
  [[nodiscard]] bool hasMember(const std::string& name) const;

  /**
   * If type is a struct - get member index by `name`
   */
  [[nodiscard]] size_t getMemberIndex(const std::string& name) const;

  /**
   * If type is a struct - get member type by `name`
   */
  [[nodiscard]] std::shared_ptr<Type> getMemberType(const std::string& name) const;

  /**
   * Generate LLVM type from a valid meta type, needs ModuleContext
   */
  [[nodiscard]] llvm::Type * getLLVMType(codegen::ModuleContext& ctx) const;

  /**
   * Generate pretty string for a type
   */
  [[nodiscard]] std::string toString() const;

  [[nodiscard]] bool isVoid() const;
  [[nodiscard]] bool isSigned() const;
  [[nodiscard]] bool isUnsigned() const;
  [[nodiscard]] bool isInteger() const;
  [[nodiscard]] bool isFloat() const;
  [[nodiscard]] bool isPointer() const;
  [[nodiscard]] bool isStruct() const;

  int getNumberBitWidth() const;

  /**
   * Generates llvm::Value * (specifically llvm::Constant *) containing default
   * value for a valid meta type
   */
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
  static std::shared_ptr<Type> createStruct(StructMembers members);

  /**
   * Saves user-defined type to customTypes
   */
  static void registerCustomType(const std::string& name, std::shared_ptr<Type> type);

  /**
   * Compares tag of lhs & rhs and returns 'bigger' type to avoid implicit downcasts
   */
  static std::shared_ptr<Type> alignTypes(std::shared_ptr<Type> lhs, std::shared_ptr<Type> rhs);
};

};

#pragma once

#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <unordered_map>
#include <vector>
#include <string>
#include "xcc/ast/node.h"

namespace xcc::codegen {
class GlobalContext;
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
  BOOL,     /** boolean type */
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
  ISIZE,    /** Signed platform-dependent size type, (equivalent to ssize_t in C) */
  USIZE,    /** Unsigned platform-dependent size type, (equivalent to size_t in C) */
  ARRAY,    /** Array type */
  PTR,      /** Generic/Opaque pointer type */
  FUNCTION, /** Function type */
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
class Type : public std::enable_shared_from_this<Type> {
private:
  TypeTag tag;

  /** For TypeTag::ARRAY */
  struct {
    std::shared_ptr<Type> elementType;
    size_t                size;
  } arr;

  /** For TypeTag::PTR */
  struct {
    std::shared_ptr<Type> pointedType;
  } ptr;

  /** For TypeTag::FUNCTION */
  struct {
    std::shared_ptr<Type>              returnType;
    std::vector<std::shared_ptr<Type>> args;
    bool                               isVariadic;
  } fn;

  /** For TypeTag::STRUCT */
  struct {
    std::string   name;
    StructMembers members;
    bool          packed;
  } _struct;

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
   * If type is an array - return element type
   */
  [[nodiscard]] std::shared_ptr<Type> getElementType() const;

  /**
   * For pointers - returns pointed type, for arrays - element type
   * If neither - will crash
   */
  [[nodiscard]] std::shared_ptr<Type> getBaseType() const;

  /**
   * Returns base type of a recursive pointer type ('i8***' -> 'i8')
   */
  [[nodiscard]] std::shared_ptr<Type> getRootType();

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
   * Add member to a struct
   */
  void addMember(const std::string& name, const std::shared_ptr<Type>& type);

  /**
   * If type in a function - get return type
   */
  [[nodiscard]] std::shared_ptr<Type> getReturnType() const;

  /**
   * If type in a function - get count of arguments
   */
  [[nodiscard]] size_t getArgumentCount() const;

  /**
   * If type in a function - get argument type by index
   */
  [[nodiscard]] std::shared_ptr<Type> getArgumentType(size_t i) const;

  /**
   * If type is a function - get isVariadic flag
   */
  [[nodiscard]] bool isVariadic() const;

  /**
   * If type is an array - get element count
   */
  [[nodiscard]] size_t getElementCount() const;

  /**
   * Generate LLVM type from a valid meta type, needs ModuleContext
   */
  [[nodiscard]] llvm::Type * getLLVMType(codegen::ModuleContext& ctx) const;

  /**
   * Generate LLVM type from a valid meta funtion type, needs ModuleContext
   */
  [[nodiscard]] llvm::FunctionType * getLLVMFunctionType(codegen::ModuleContext& ctx) const;

  /**
   * Generate LLVM type from a valid meta array type, needs ModuleContext
   */
  [[nodiscard]] llvm::ArrayType * getLLVMArrayType(codegen::ModuleContext& ctx) const;

  /**
   * Generate LLVM DebugInfo Type
   */
  [[nodiscard]] llvm::DIType * getDIType(codegen::ModuleContext& ctx) const;

  /**
   * Generate LLVM DebugInfo Function Type
   */
  [[nodiscard]] llvm::DISubroutineType * getDISubroutineType(codegen::ModuleContext& ctx) const;

  /**
   * Returns type name. If struct - struct name, otherwise - just type name
   */
  [[nodiscard]] std::string getName() const;

  /**
   * Generate pretty string for a type
   */
  [[nodiscard]] std::string toString() const;

  [[nodiscard]] bool isVoid() const;
  [[nodiscard]] bool iBool() const;
  [[nodiscard]] bool isSigned() const;
  [[nodiscard]] bool isUnsigned() const;
  [[nodiscard]] bool isInteger() const;
  [[nodiscard]] bool isFloat() const;
  [[nodiscard]] bool isArray() const;
  [[nodiscard]] bool isPointer() const;
  [[nodiscard]] bool isFunction() const;
  [[nodiscard]] bool isStruct() const;

  int getNumberBitWidth() const;

  /**
   * Generates llvm::Value * (specifically llvm::Constant *) containing default
   * value for a valid meta type
   */
  llvm::Value * getDefault(codegen::ModuleContext& ctx) const;

  /**
   * Check if type's tag matches provided one
   */
  bool is(TypeTag expected) const {
    return tag == expected;
  }

  /**
   * Check if type's tag matches any of the provided one's
   */
  template <typename ...Types>
  bool isAnyOf(Types... expected) const {
    return ((this->tag == expected) || ...);
  }

  /**
   * Generate AST Type Node from meta type
   */
  std::shared_ptr<ast::Node> toAst(SourceSpan span = SourceSpan::builtin()) const;

  /**
   * Create an empty type tagged with `tag`
   *
   * @note Internal helper
   * @warning By itself produces malformed types
   */
  static std::shared_ptr<Type> create(TypeTag tag);

  /**
   * Try to create a meta::Type from string name
   */
  static std::shared_ptr<Type> fromTypeName(codegen::GlobalContext& ctx, const std::string& name, SourceSpan span);

  static std::shared_ptr<Type> createVoid();
  static std::shared_ptr<Type> createBool();
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
  static std::shared_ptr<Type> createIsize();
  static std::shared_ptr<Type> createUsize();
  static std::shared_ptr<Type> createSigned(int bits);
  static std::shared_ptr<Type> createUnsigned(int bits);
  static std::shared_ptr<Type> createFloating(int bits);
  static std::shared_ptr<Type> createArray(std::shared_ptr<Type> elementType, size_t size);
  static std::shared_ptr<Type> createPointer(std::shared_ptr<Type> pointedType);
  static std::shared_ptr<Type> createFunction(
    std::shared_ptr<Type> returnType, std::vector<std::shared_ptr<Type>> args, bool isVariadic = false);
  static std::shared_ptr<Type> createStruct(std::string name, StructMembers members, bool packed = false);

  /**
   * Try to infer meta::Type from ast::Node
   */
  static std::shared_ptr<Type> inferFromNode(codegen::ModuleContext& ctx, std::shared_ptr<ast::Node> node);

  /**
   * Saves user-defined type to customTypes
   */
  static void registerCustomType(const std::string& name, std::shared_ptr<Type> type);

  /**
   * Check if custom type with specified type is present
   */
  static bool hasCustomType(const std::string& name);

  /**
   * Compares tag of lhs & rhs and returns 'bigger' type to avoid implicit downcasts
   */
  static std::shared_ptr<Type> alignTypes(std::shared_ptr<Type> lhs, std::shared_ptr<Type> rhs);
};

};

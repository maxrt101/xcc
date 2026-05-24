#include "xcc/meta/type.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/util/string.h"
#include "xcc/util/util.h"

using namespace xcc;
using namespace xcc::meta;

// TODO: Should be scoped per-module (or per-file)!
std::unordered_map<std::string, std::shared_ptr<Type>> Type::customTypes;

static llvm::DICompositeType * createDIEnumerator(
  codegen::ModuleContext& ctx,
  const std::string&      name,
  std::shared_ptr<Type>   base,
  std::vector<EnumField> members
) {
  auto& dl = ctx.llvm.module->getDataLayout();
  auto llvmType = base->getLLVMType(ctx);

  uint64_t structSize  = dl.getTypeAllocSizeInBits(llvmType);
  uint32_t structAlign = dl.getABITypeAlign(llvmType).value() * 8;

  std::vector<llvm::Metadata*> enumMembers;

  for (size_t i = 0; i < members.size(); ++i) {
    auto& member = members[i];

    enumMembers.push_back(ctx.globalContext.di_builder->createEnumerator(
      member.name,
      member.value,
      base->isUnsigned()
    ));
  }

  return ctx.globalContext.di_builder->createEnumerationType(
    ctx.currentDIScope(),
    name,
    ctx.globalContext.getCurrentDIFile(),
    0, // FIXME
    structSize,
    structAlign,
    ctx.globalContext.di_builder->getOrCreateArray(enumMembers),
    base->getDIType(ctx)
  );
}

/**
 * Helper to create DIDerivedType (struct member)
 *
 * @param ctx      ModuleContext
 * @param llvmType llvm::Type of composite (parent/struct) type
 * @param index    Member index
 * @param name     Member name
 * @param type     Member meta type
 */
static llvm::DIDerivedType * createDIDerivedType(
  codegen::ModuleContext& ctx,
  llvm::Type *            llvmType,
  size_t                  index,
  const std::string&      name,
  std::shared_ptr<Type>   type
) {
  auto& dl = ctx.llvm.module->getDataLayout();

  uint64_t memberSize  = dl.getTypeAllocSizeInBits(type->getLLVMType(ctx));
  uint32_t memberAlign = dl.getABITypeAlign(type->getLLVMType(ctx)).value() * 8;

  // Calculate bit offset using DataLayout
  uint64_t offsetInBits = dl.getStructLayout(llvm::cast<llvm::StructType>(llvmType))->getElementOffsetInBits(index);

  return ctx.globalContext.di_builder->createMemberType(
      ctx.currentDIScope(),
      name,
      ctx.globalContext.di_compile_unit->getFile(),
      0, // FIXME
      memberSize,
      memberAlign,
      offsetInBits,
      llvm::DINode::FlagZero,
      type->getDIType(ctx)
  );
}

/**
 * Helper to create DICompositeType (struct type)
 *
 * @param ctx      Module Context
 * @param llvmType llvm::Type of composite (struct) type
 * @param name     Struct name
 * @param members  Struct members metadata
 */
static llvm::DICompositeType * createDICompositeType(
  codegen::ModuleContext&            ctx,
  llvm::Type *                       llvmType,
  const std::string&                 name,
  const std::vector<llvm::Metadata*> members
) {
  auto& dl = ctx.llvm.module->getDataLayout();

  uint64_t structSize  = dl.getTypeAllocSizeInBits(llvmType);
  uint32_t structAlign = dl.getABITypeAlign(llvmType).value() * 8;

  return ctx.globalContext.di_builder->createStructType(
      ctx.currentDIScope(),
      name,
      ctx.globalContext.getCurrentDIFile(),
      0, // FIXME
      structSize,
      structAlign,
      llvm::DINode::FlagZero,
      nullptr,
      ctx.globalContext.di_builder->getOrCreateArray(members)
  );
}

Type::Type(TypeTag tag) : tag(tag) {}

bool Type::operator==(Type& rhs) {
  if (tag != rhs.tag) {
    return false;
  }

  if (tag == TypeTag::PTR) {
    return *ptr.pointedType == *rhs.ptr.pointedType;
  }

  if (tag == TypeTag::STRUCT) {
    for (size_t i = 0; i < _struct.members.size(); ++i) {
      if ( _struct.members[i].first   !=  rhs._struct.members[i].first
        || *_struct.members[i].second != *rhs._struct.members[i].second) {
        return false;
      }
    }
  }

  if (tag == TypeTag::ENUM) {
    if (*_enum.base != *rhs._enum.base) {
      return false;
    }

    for (size_t i = 0; i < _enum.members.size(); ++i) {
      if ( _enum.members[i].name  != rhs._enum.members[i].name
        || _enum.members[i].value != rhs._enum.members[i].value) {
        return false;
      }
    }
  }

  if (tag == TypeTag::TUPLE) {
    for (size_t i = 0; i < tuple.members.size(); ++i) {
      if (*tuple.members[i] != *rhs.tuple.members[i]) {
        return false;
      }
    }
  }

  if (tag == TypeTag::LAMBDA) {
    if (*lambda.fn != *rhs.lambda.fn) {
      return false;
    }

    for (size_t i = 0; i < lambda.captures.size(); ++i) {
      if (*lambda.captures[i] != *rhs.lambda.captures[i]) {
        return false;
      }
    }
  }

  if (tag == TypeTag::FUNCTION) {
    if (*fn.returnType != *rhs.fn.returnType) {
      return false;
    }

    if (fn.isVariadic != rhs.fn.isVariadic) {
      return false;
    }

    if (fn.args.size() != rhs.fn.args.size()) {
      return false;
    }

    for (size_t i = 0; i < fn.args.size(); ++i) {
      if (*fn.args[i] != *rhs.fn.args[i]) {
        return false;
      }
    }
  }

  return true;
}

bool Type::operator!=(Type& rhs) {
  return !(*this == rhs);
}

TypeTag Type::getTag() const {
  return tag;
}

std::shared_ptr<Type> Type::getPointedType() const {
  assertThrow(isPointer(), std::runtime_error("getPointedType called on a non-pointer type"));

  return ptr.pointedType;
}

std::shared_ptr<Type> Type::getElementType() const {
  assertThrow(isArray(), std::runtime_error("getElementType called on a non-array type"));

  return arr.elementType;
}

std::shared_ptr<Type> Type::getEnumElementType() const {
  assertThrow(isEnum(), std::runtime_error("getEnumElementType called on a non-array type"));

  return _enum.base;
}

std::shared_ptr<Type> Type::getBaseType() const {
  if (isArray()) {
    return getElementType();
  }

  if (isPointer()) {
    return getPointedType();
  }

  if (isEnum()) {
    return getEnumElementType();
  }

  throw std::runtime_error("Type is not a pointer or an array");
}

std::shared_ptr<Type> Type::getRootType() {
  if (isPointer()) {
    return ptr.pointedType->getRootType();
  }

  return shared_from_this();
}

llvm::Type * Type::getLLVMType(codegen::ModuleContext& ctx) const {
  switch (tag) {
    case TypeTag::VOID:
      return llvm::Type::getVoidTy(*ctx.llvm.ctx);

    case TypeTag::BOOL:
      return llvm::Type::getInt1Ty(*ctx.llvm.ctx);

    case TypeTag::U8:
    case TypeTag::I8:
      return llvm::IntegerType::get(*ctx.llvm.ctx, 8);

    case TypeTag::U16:
    case TypeTag::I16:
      return llvm::IntegerType::get(*ctx.llvm.ctx, 16);

    case TypeTag::U32:
    case TypeTag::I32:
      return llvm::IntegerType::get(*ctx.llvm.ctx, 32);

    case TypeTag::U64:
    case TypeTag::I64:
      return llvm::IntegerType::get(*ctx.llvm.ctx, 64);

    case TypeTag::F32:
      return llvm::Type::getFloatTy(*ctx.llvm.ctx);

    case TypeTag::F64:
      return llvm::Type::getDoubleTy(*ctx.llvm.ctx);

    case TypeTag::ISIZE:
    case TypeTag::USIZE: {
      auto bits = ctx.llvm.module->getDataLayout().getPointerSizeInBits();
      return llvm::IntegerType::get(*ctx.llvm.ctx, bits);
    }

    case TypeTag::ARRAY:
      return getLLVMArrayType(ctx);

    case TypeTag::PTR:
      return llvm::PointerType::get(ptr.pointedType->getLLVMType(ctx), 0);

    case TypeTag::FUNCTION:
      return llvm::StructType::get(*ctx.llvm.ctx, {
        ctx.ir_builder->getPtrTy(), // [0] Code pointer
        ctx.ir_builder->getPtrTy()  // [1] Closure environment pointer
      });

    case TypeTag::LAMBDA: {
      std::vector<llvm::Type *> elements;

      for (auto & member : lambda.captures) {
        elements.push_back(member->getLLVMType(ctx));
      }

      return llvm::StructType::get(*ctx.llvm.ctx, elements, false);
    }

    case TypeTag::ENUM:
      return _enum.base->getLLVMType(ctx);

    case TypeTag::TUPLE: {
      std::vector<llvm::Type *> elements;

      for (auto & member : tuple.members) {
        elements.push_back(member->getLLVMType(ctx));
      }

      return llvm::StructType::get(*ctx.llvm.ctx, elements, false);
    }

    case TypeTag::STRUCT: {
      std::vector<llvm::Type *> elements;

      for (auto & member : _struct.members) {
        elements.push_back(member.second->getLLVMType(ctx));
      }

      return llvm::StructType::get(*ctx.llvm.ctx, elements, _struct.packed);
    }

    default:
      throw std::runtime_error("Unknown type");
  }
}

llvm::FunctionType * Type::getLLVMFunctionType(codegen::ModuleContext& ctx) const {
  assertThrow(isFunction(), std::runtime_error("getLLVMFunctionType called on a non-function type"));

  std::vector<llvm::Type*> args;

  for (auto& p : fn.args) {
    args.push_back(p->getLLVMType(ctx));
  }

  return llvm::FunctionType::get(
    fn.returnType->getLLVMType(ctx),
    args,
    fn.isVariadic
  );
}

llvm::ArrayType * Type::getLLVMArrayType(codegen::ModuleContext& ctx) const {
  assertThrow(isArray(), std::runtime_error("getLLVMArrayType called on non-array type"));

  return llvm::ArrayType::get(arr.elementType->getLLVMType(ctx), arr.size);
}

llvm::DIType * Type::getDIType(codegen::ModuleContext& ctx) const {
  switch (tag) {
    case TypeTag::VOID:   return ctx.globalContext.di_builder->createBasicType("void",  0,  0);
    case TypeTag::BOOL:   return ctx.globalContext.di_builder->createBasicType("bool",  1,  llvm::dwarf::DW_ATE_boolean);
    case TypeTag::U8:     return ctx.globalContext.di_builder->createBasicType("u8",    8,  llvm::dwarf::DW_ATE_unsigned);
    case TypeTag::I8:     return ctx.globalContext.di_builder->createBasicType("i8",    8,  llvm::dwarf::DW_ATE_signed);
    case TypeTag::U16:    return ctx.globalContext.di_builder->createBasicType("u16",   16, llvm::dwarf::DW_ATE_unsigned);
    case TypeTag::I16:    return ctx.globalContext.di_builder->createBasicType("i16",   16, llvm::dwarf::DW_ATE_signed);
    case TypeTag::U32:    return ctx.globalContext.di_builder->createBasicType("u32",   32, llvm::dwarf::DW_ATE_unsigned);
    case TypeTag::I32:    return ctx.globalContext.di_builder->createBasicType("i32",   32, llvm::dwarf::DW_ATE_signed);
    case TypeTag::U64:    return ctx.globalContext.di_builder->createBasicType("u64",   64, llvm::dwarf::DW_ATE_unsigned);
    case TypeTag::I64:    return ctx.globalContext.di_builder->createBasicType("u64",   64, llvm::dwarf::DW_ATE_signed);
    case TypeTag::F32:    return ctx.globalContext.di_builder->createBasicType("f32",   32, llvm::dwarf::DW_ATE_float);
    case TypeTag::F64:    return ctx.globalContext.di_builder->createBasicType("f64",   64, llvm::dwarf::DW_ATE_float);
    case TypeTag::ISIZE:  return ctx.globalContext.di_builder->createBasicType("isize", 64, llvm::dwarf::DW_ATE_signed);   // FIXME: Bitwidth Hardcode
    case TypeTag::USIZE:  return ctx.globalContext.di_builder->createBasicType("usize", 64, llvm::dwarf::DW_ATE_unsigned); // FIXME: Bitwidth Hardcode

    case TypeTag::ARRAY: {
      return ctx.globalContext.di_builder->createArrayType(
        arr.size,
        8, // FIXME: Alignment hardcode
        arr.elementType->getDIType(ctx),
        ctx.globalContext.di_builder->getOrCreateArray({
          ctx.globalContext.di_builder->getOrCreateSubrange(0, arr.size)
        })
      );
    }

    case TypeTag::PTR: {
      return ctx.globalContext.di_builder->createPointerType(ptr.pointedType->getDIType(ctx), getLLVMType(ctx)->getScalarSizeInBits());
    }

    case TypeTag::FUNCTION: {
      auto& dl = ctx.llvm.module->getDataLayout();
      uint64_t ptrSize = dl.getPointerSizeInBits();
      uint32_t ptrAlign = dl.getPointerABIAlignment(0).value() * 8;

      return ctx.globalContext.di_builder->createPointerType(getDISubroutineType(ctx), ptrSize, ptrAlign);
    }

    case TypeTag::LAMBDA: {
      auto * llvmType = getLLVMType(ctx);

      std::vector<llvm::Metadata *> captures;

      for (size_t i = 0; i < lambda.captures.size(); ++i) {
        auto& capture = lambda.captures[i];
        captures.push_back(createDIDerivedType(ctx, llvmType, i, std::to_string(i), capture));
      }

      return createDICompositeType(ctx, llvmType, toString(), captures);
    }

    case TypeTag::ENUM: {
      return createDIEnumerator(ctx, _enum.name, _enum.base, _enum.members);
    }

    case TypeTag::TUPLE: {
      auto * llvmType = getLLVMType(ctx);

      std::vector<llvm::Metadata *> members;

      for (size_t i = 0; i < tuple.members.size(); ++i) {
        auto& member = tuple.members[i];
        members.push_back(createDIDerivedType(ctx, llvmType, i, std::to_string(i), member));
      }

      return createDICompositeType(ctx, llvmType, toString(), members);
    }

    case TypeTag::STRUCT: {
      auto * llvmType = getLLVMType(ctx);

      std::vector<llvm::Metadata *> members;

      for (size_t i = 0; i < _struct.members.size(); ++i) {
        auto& member = _struct.members[i];
        members.push_back(createDIDerivedType(ctx, llvmType, i, member.first, member.second));
      }

      return createDICompositeType(ctx, llvmType, _struct.name, members);
    }

    default:
      return nullptr;
  }
}

llvm::DISubroutineType * Type::getDISubroutineType(codegen::ModuleContext& ctx) const {
  assertThrow(isAnyOf(TypeTag::FUNCTION, TypeTag::LAMBDA), std::runtime_error("getDISubroutineType called on non-function type"));

  std::vector<llvm::Metadata *> typeEltArray;

  typeEltArray.push_back(getReturnType()->getDIType(ctx));

  for (auto& argType : getArgumentTypes()) {
    typeEltArray.push_back(argType->getDIType(ctx));
  }

  llvm::DITypeRefArray typeArray = ctx.globalContext.di_builder->getOrCreateTypeArray(typeEltArray);

  return ctx.globalContext.di_builder->createSubroutineType(typeArray);
}

std::string Type::getName() const {
  if (tag == TypeTag::STRUCT) {
    return _struct.name;
  }

  if (tag == TypeTag::ENUM) {
    return _enum.name;
  }

  return toString();
}

std::string Type::toString() const {
  switch (tag) {
    case TypeTag::VOID:   return "void";
    case TypeTag::BOOL:   return "bool";
    case TypeTag::U8:     return "u8";
    case TypeTag::I8:     return "i8";
    case TypeTag::U16:    return "u16";
    case TypeTag::I16:    return "i16";
    case TypeTag::U32:    return "u32";
    case TypeTag::I32:    return "i32";
    case TypeTag::U64:    return "u64";
    case TypeTag::I64:    return "i64";
    case TypeTag::F32:    return "f32";
    case TypeTag::F64:    return "f64";
    case TypeTag::ISIZE:  return "isize";
    case TypeTag::USIZE:  return "usize";
    case TypeTag::ARRAY:  return std::format("{}[{}]", arr.elementType->toString(), arr.size);
    case TypeTag::PTR:    return ptr.pointedType->toString() + "*";

    case TypeTag::FUNCTION: {
      std::string result = "fn (";

      for (size_t i = 0; i < fn.args.size(); ++i) {
        result += fn.args[i]->toString();
        if (i + 1 < fn.args.size()) {
          result += ", ";
        }
      }

      return result + "): " + fn.returnType->toString();
    }

    case TypeTag::LAMBDA: {
      std::string result = "fn [";

      for (size_t i = 0; i < lambda.captures.size(); ++i) {
        auto& capture = lambda.captures[i];
        result += capture->toString();
        if (i + 1 < lambda.captures.size()) {
          result += ", ";
        }
      }

      result += "] (";

      for (size_t i = 0; i < lambda.fn->fn.args.size(); ++i) {
        result += lambda.fn->fn.args[i]->toString();
        if (i + 1 < lambda.fn->fn.args.size()) {
          result += ", ";
        }
      }

      return result + "): " + lambda.fn->fn.returnType->toString();
    }

    case TypeTag::ENUM: {
      std::string result = "enum : " + _enum.base->toString() + " {";

      for (size_t i = 0; i < _enum.members.size(); ++i) {
        result += _enum.members[i].name + " = " + std::to_string(_enum.members[i].value);
        if (i + 1 < _enum.members.size()) {
          result += ", ";
        }
      }

      return result += "}";
    }

    case TypeTag::TUPLE: {
      std::string result = "[";

      for (size_t i = 0; i < tuple.members.size(); ++i) {
        result += tuple.members[i]->toString();
        if (i + 1 < tuple.members.size()) {
          result += ", ";
        }
      }

      return result + "]";
    }

    case TypeTag::STRUCT: {
      std::string result;

      if (_struct.packed) {
        result += "[packed] ";
      }

      result += "struct {";
      for (size_t i = 0; i < _struct.members.size(); ++i) {
        result += _struct.members[i].first;
        result += ": ";
        result += _struct.members[i].second->toString();
        if (i + 1 != _struct.members.size()) {
          result += ", ";
        }
      }
      return result + "}";
    }

    default:
      return "<?>";
  }
}

bool Type::isVoid() const {
  return is(TypeTag::VOID);
}

bool Type::iBool() const {
  return is(TypeTag::BOOL);
}

bool Type::isSigned() const {
  return isAnyOf(TypeTag::I8, TypeTag::I16, TypeTag::I32, TypeTag::I64, TypeTag::ISIZE);
}

bool Type::isUnsigned() const {
  return isAnyOf(TypeTag::U8, TypeTag::U16, TypeTag::U32, TypeTag::U64, TypeTag::USIZE);
}

bool Type::isInteger() const {
  return isAnyOf(TypeTag::I8, TypeTag::I16, TypeTag::I32, TypeTag::I64,
                 TypeTag::U8, TypeTag::U16, TypeTag::U32, TypeTag::U64,
                 TypeTag::ISIZE, TypeTag::USIZE);
}

bool Type::isFloat() const {
  return isAnyOf(TypeTag::F32, TypeTag::F64);
}

bool Type::isArray() const {
  return is(TypeTag::ARRAY);
}

bool Type::isPointer() const {
  return is(TypeTag::PTR);
}

bool Type::isFunction() const {
  return is(TypeTag::FUNCTION);
}

bool Type::isLambda() const {
  return is(TypeTag::LAMBDA);
}

bool Type::isEnum() const {
  return is(TypeTag::ENUM);
}

bool Type::isTuple() const {
  return is(TypeTag::TUPLE);
}

bool Type::isStruct() const {
  return is(TypeTag::STRUCT);
}

int Type::getNumberBitWidth() const {
  switch (tag) {
    case TypeTag::BOOL:
      return 1;
    case TypeTag::U8:
    case TypeTag::I8:
      return 8;
    case TypeTag::U16:
    case TypeTag::I16:
      return 16;
    case TypeTag::U32:
    case TypeTag::I32:
    case TypeTag::F32:
      return 32;
    case TypeTag::I64:
    case TypeTag::U64:
    case TypeTag::F64:
    case TypeTag::ISIZE: // FIXME: Hardcoded bit width for isize/usize
    case TypeTag::USIZE:
      return 64;
    case TypeTag::PTR:
    case TypeTag::FUNCTION:
    case TypeTag::LAMBDA:
    case TypeTag::ENUM:
    case TypeTag::TUPLE:
    case TypeTag::STRUCT:
    case TypeTag::VOID:
    default:
      return 0;
  }
}

bool Type::hasMember(const std::string& name) const {
  assertThrow(isStruct(), std::runtime_error("Type '" + toString() + "' is not a struct"));

  return std::find_if(
    _struct.members.begin(), _struct.members.end(),
    [&name](auto & element) {
      return element.first == name;
    }
  ) != _struct.members.end();
}

size_t Type::getMemberIndex(const std::string& name) const {
  assertThrow(isStruct(), std::runtime_error("Type is not a struct"));

  for (size_t idx = 0; idx < _struct.members.size(); ++idx) {
    if (_struct.members[idx].first == name) {
      return idx;
    }
  }

  Error(ERROR_UNKNOWN_MEMBER, {}, "Struct '" + toString() + "' has no member '" + name + "'").raise();
}

std::shared_ptr<Type> Type::getMemberType(const std::string& name) const {
  assertThrow(isStruct(), std::runtime_error("Type is not a struct"));

  for (size_t idx = 0; idx < _struct.members.size(); ++idx) {
    if (_struct.members[idx].first == name) {
      return _struct.members[idx].second;
    }
  }

  Error(ERROR_UNKNOWN_MEMBER, {}, "Struct '{}' has no member '{}'", toString(), name).raise();
}

std::string Type::getMemberName(size_t index) const {
  assertThrow(isStruct(), std::runtime_error("Type is not a struct"));
  assertThrow(index < _struct.members.size(), std::runtime_error("Out of bounds struct member name request"));

  return _struct.members[index].first;
}

void Type::addMember(const std::string& name, const std::shared_ptr<Type>& type) {
  assertThrow(isStruct(), std::runtime_error("Type is not a struct"));

  _struct.members.emplace_back(name, type);
}

std::shared_ptr<Type> Type::getReturnType() const {
  assertThrow(isFunction() || isLambda(), std::runtime_error("Type is not a function or a lambda"));

  return isFunction() ? fn.returnType : lambda.fn->fn.returnType;
}

size_t Type::getArgumentCount() const {
  assertThrow(isFunction() || isLambda(), std::runtime_error("Type is not a function or a lambda"));

  return isFunction() ? fn.args.size() : lambda.fn->fn.args.size();
}

std::shared_ptr<Type> Type::getArgumentType(size_t i) const {
  assertThrow(isFunction() || isLambda(), std::runtime_error("Type is not a function or a lambda"));
  assertThrow(isFunction() ? i < fn.args.size() : lambda.fn->fn.args.size(),
    std::runtime_error("Out of bounds function/lambda argument type request"));

  return isFunction() ? fn.args[i] : lambda.fn->fn.args[i];
}

std::vector<std::shared_ptr<Type>> Type::getArgumentTypes() const {
  assertThrow(isFunction() || isLambda(), std::runtime_error("Type is not a function or a lambda"));

  return isFunction() ? fn.args : lambda.fn->fn.args;
}

bool Type::isVariadic() const {
  assertThrow(isFunction() || isLambda() || isLambda(), std::runtime_error("Type is not a function or a lambda"));

  return isFunction() ? fn.isVariadic : lambda.fn->fn.isVariadic;
}

size_t Type::getElementCount() const {
  assertThrow(isArray(), std::runtime_error("Type is not an array"));

  return arr.size;
}

size_t Type::getTupleMemberCount() const {
  assertThrow(isTuple(), std::runtime_error("Type is not a tuple"));

  return tuple.members.size();
}

std::shared_ptr<Type> Type::getTupleMemberType(size_t i) const {
  assertThrow(isTuple(), std::runtime_error("Type is not a tuple"));
  assertThrow(i < tuple.members.size(), std::runtime_error("Out of bounds tuple member type request"));

  return tuple.members[i];
}

size_t Type::getEnumElementCount() const {
  assertThrow(isEnum(), std::runtime_error("Type is not an enum"));

  return _enum.members.size();
}

EnumField Type::getEnumElement(size_t i) const {
  assertThrow(isEnum(), std::runtime_error("Type is not an enum"));
  assertThrow(i < _enum.members.size(), std::runtime_error("Out of bounds enum member value request"));

  return _enum.members[i];
}

EnumField Type::getEnumElement(const std::string& name) const {
  assertThrow(isEnum(), std::runtime_error("Type is not an enum"));

  for (size_t i = 0; i < _enum.members.size(); ++i) {
    if (_enum.members[i].name == name) {
      return _enum.members[i];
    }
  }

  throw std::runtime_error("No such enum member " + name + " for enum " + _enum.name);
}

bool Type::hasEnumElement(const std::string& name) const {
  assertThrow(isEnum(), std::runtime_error("Type is not an enum"));

  for (size_t i = 0; i < _enum.members.size(); ++i) {
    if (_enum.members[i].name == name) {
      return true;
    }
  }

  return false;
}

void Type::addEnumElement(EnumField field) {
  _enum.members.push_back(std::move(field));
}

std::shared_ptr<Type> Type::getCaptureType(size_t i) const {
  assertThrow(isLambda(), std::runtime_error("Type is not a lambda"));
  assertThrow(i < lambda.captures.size(), std::runtime_error("Out of bounds capture type request"));

  return lambda.captures[i];
}

std::shared_ptr<Type> Type::getLambdaFunctionType() const {
  assertThrow(isLambda(), std::runtime_error("Type is not a lambda"));

  return lambda.fn;
}

llvm::Constant * Type::getDefault(codegen::ModuleContext& ctx) const {
  switch (tag) {
    case TypeTag::BOOL:
    case TypeTag::U8:
    case TypeTag::U16:
    case TypeTag::U32:
    case TypeTag::U64:
    case TypeTag::I8:
    case TypeTag::I16:
    case TypeTag::I32:
    case TypeTag::I64:
    case TypeTag::ISIZE:
    case TypeTag::USIZE:
      return llvm::ConstantInt::get(getLLVMType(ctx), 0);

    case TypeTag::F32:
    case TypeTag::F64:
      return llvm::ConstantFP::get(getLLVMType(ctx), 0.0);

    case TypeTag::ARRAY: {
      return llvm::ConstantAggregateZero::get(getLLVMArrayType(ctx));
    }

    case TypeTag::PTR:
      return llvm::Constant::getNullValue(getLLVMType(ctx));

    case TypeTag::FUNCTION: {
      return llvm::Constant::getNullValue(getLLVMType(ctx));
    }

    case TypeTag::LAMBDA: {
      std::vector<llvm::Constant *> initializers;

      for (auto & capture : lambda.captures) {
        initializers.push_back((llvm::Constant *) capture->getDefault(ctx));
      }

      return llvm::ConstantStruct::get((llvm::StructType *) getLLVMType(ctx), initializers);
    }

    case TypeTag::ENUM:
      return _enum.members.empty()
        ? _enum.base->getDefault(ctx)
        : llvm::ConstantInt::get(_enum.base->getLLVMType(ctx), _enum.members[0].value);

    case TypeTag::TUPLE: {
      std::vector<llvm::Constant *> initializers;

      for (auto & member : tuple.members) {
        initializers.push_back((llvm::Constant *) member->getDefault(ctx));
      }

      return llvm::ConstantStruct::get((llvm::StructType *) getLLVMType(ctx), initializers);
    }

    case TypeTag::STRUCT: {
      std::vector<llvm::Constant *> initializers;

      for (auto & member : _struct.members) {
        initializers.push_back((llvm::Constant *) member.second->getDefault(ctx));
      }

      return llvm::ConstantStruct::get((llvm::StructType *) getLLVMType(ctx), initializers);
    }

    case TypeTag::VOID:
    default:
      return nullptr;
  }
}

std::shared_ptr<ast::Node> Type::toAst(SourceSpan span, ast::LexicalScope lexicalScope) const {
  switch (tag) {
    case TypeTag::VOID:  return ast::Type::create(span, lexicalScope, ast::Identifier::create(span, lexicalScope, "void"));
    case TypeTag::BOOL:  return ast::Type::create(span, lexicalScope, ast::Identifier::create(span, lexicalScope, "bool"));
    case TypeTag::U8:    return ast::Type::create(span, lexicalScope, ast::Identifier::create(span, lexicalScope, "u8"));
    case TypeTag::U16:   return ast::Type::create(span, lexicalScope, ast::Identifier::create(span, lexicalScope, "u16"));
    case TypeTag::U32:   return ast::Type::create(span, lexicalScope, ast::Identifier::create(span, lexicalScope, "u32"));
    case TypeTag::U64:   return ast::Type::create(span, lexicalScope, ast::Identifier::create(span, lexicalScope, "u64"));
    case TypeTag::I8:    return ast::Type::create(span, lexicalScope, ast::Identifier::create(span, lexicalScope, "i8"));
    case TypeTag::I16:   return ast::Type::create(span, lexicalScope, ast::Identifier::create(span, lexicalScope, "i16"));
    case TypeTag::I32:   return ast::Type::create(span, lexicalScope, ast::Identifier::create(span, lexicalScope, "i32"));
    case TypeTag::I64:   return ast::Type::create(span, lexicalScope, ast::Identifier::create(span, lexicalScope, "i64"));
    case TypeTag::F32:   return ast::Type::create(span, lexicalScope, ast::Identifier::create(span, lexicalScope, "f32"));
    case TypeTag::F64:   return ast::Type::create(span, lexicalScope, ast::Identifier::create(span, lexicalScope, "f64"));
    case TypeTag::ISIZE: return ast::Type::create(span, lexicalScope, ast::Identifier::create(span, lexicalScope, "isize"));
    case TypeTag::USIZE: return ast::Type::create(span, lexicalScope, ast::Identifier::create(span, lexicalScope, "usize"));
    case TypeTag::PTR:   return ast::Type::createPointer(span, lexicalScope, ptr.pointedType->toAst());

    case TypeTag::ARRAY: {
      return ast::Type::createArray(
        span, lexicalScope,
        arr.elementType->toAst(),
        ast::Number::createInteger(span, lexicalScope, arr.size)
      );
    }

    case TypeTag::FUNCTION: {
      ast::NodeList args;

      for (auto& arg : fn.args) {
        args.push_back(arg->toAst());
      }

      return ast::Type::createFunction(span, lexicalScope, fn.returnType->toAst(), args, fn.isVariadic);
    }

    case TypeTag::LAMBDA: {
      ast::NodeList captures, args;

      for (auto& capture : lambda.captures) {
        args.push_back(capture->toAst(span));
      }

      for (auto& arg : lambda.fn->fn.args) {
        args.push_back(arg->toAst());
      }

      return ast::Type::createLambda(span, lexicalScope, captures, lambda.fn->fn.returnType->toAst(), args, lambda.fn->fn.isVariadic);
    }

    case TypeTag::ENUM: {
      ast::Enum::FieldList fields;

      for (auto& field : _enum.members) {
        fields.emplace_back(
          ast::Identifier::create(span, lexicalScope, field.name),
          ast::Number::createInteger(span, lexicalScope, field.value)
        );
      }

      return ast::Enum::create(span, lexicalScope, ast::Identifier::create(span, lexicalScope, _enum.name), _enum.base->toAst(span), fields);
    }

    case TypeTag::TUPLE: {
      ast::NodeList members;

      for (auto& member : tuple.members) {
        members.push_back(member->toAst());
      }

      return ast::Type::createTuple(span, lexicalScope, members);
    }

    case TypeTag::STRUCT: {
      return ast::Type::create(span, lexicalScope, ast::Identifier::create(span, lexicalScope, _struct.name));
    }

    default:
      return ast::Empty::create();
  }
}

std::shared_ptr<Type> Type::create(TypeTag tag) {
  return std::make_shared<Type>(tag);
}

std::shared_ptr<Type> Type::fromTypeName(codegen::GlobalContext& ctx, const std::string& name, SourceSpan span) {
  switch (util::strhash(name.c_str())) {
    case util::strhash("void"):  return createVoid();
    case util::strhash("bool"):  return createBool();
    case util::strhash("i8"):    return createI8();
    case util::strhash("i16"):   return createI16();
    case util::strhash("i32"):   return createI32();
    case util::strhash("i64"):   return createI64();
    case util::strhash("u8"):    return createU8();
    case util::strhash("u16"):   return createU16();
    case util::strhash("u32"):   return createU32();
    case util::strhash("u64"):   return createU64();
    case util::strhash("f32"):   return createF32();
    case util::strhash("f64"):   return createF64();
    case util::strhash("isize"): return createIsize();
    case util::strhash("usize"): return createUsize();
    default: {
      if (customTypes.find(name) != customTypes.end()) {
        return customTypes[name];
      }

      auto prefixed_name = ctx.getModulePrefix() + name;

      if (customTypes.find(prefixed_name) != customTypes.end()) {
        return customTypes[prefixed_name];
      }

      Error(ERROR_UNKNOWN_TYPE, span, "Unknown type '" + name + "'").raise();
    }
  }
}

std::shared_ptr<Type> Type::createVoid() {
  return create(TypeTag::VOID);
}

std::shared_ptr<Type> Type::createBool() {
  return create(TypeTag::BOOL);
}

std::shared_ptr<Type> Type::createI8() {
  return create(TypeTag::I8);
}

std::shared_ptr<Type> Type::createI16() {
  return create(TypeTag::I16);
}

std::shared_ptr<Type> Type::createI32() {
  return create(TypeTag::I32);
}

std::shared_ptr<Type> Type::createI64() {
  return create(TypeTag::I64);
}

std::shared_ptr<Type> Type::createU8() {
  return create(TypeTag::U8);
}

std::shared_ptr<Type> Type::createU16() {
  return create(TypeTag::U16);
}

std::shared_ptr<Type> Type::createU32() {
  return create(TypeTag::U32);
}

std::shared_ptr<Type> Type::createU64() {
  return create(TypeTag::U64);
}

std::shared_ptr<Type> Type::createF32() {
  return create(TypeTag::F32);
}

std::shared_ptr<Type> Type::createF64() {
  return create(TypeTag::F64);
}

std::shared_ptr<Type> Type::createIsize() {
  return create(TypeTag::ISIZE);
}

std::shared_ptr<Type> Type::createUsize() {
  return create(TypeTag::USIZE);
}

std::shared_ptr<Type> Type::createSigned(int bits) {
  switch (bits) {
    case 8:  return createI8();
    case 16: return createI16();
    case 32: return createI32();
    case 64:
    default: return createI64();
  }
}

std::shared_ptr<Type> Type::createUnsigned(int bits) {
  switch (bits) {
    case 8:  return createU8();
    case 16: return createU16();
    case 32: return createU32();
    case 64:
    default: return createU64();
  }
}

std::shared_ptr<Type> Type::createFloating(int bits) {
  switch (bits) {
    case 32: return createF32();
    case 64:
    default: return createF64();
  }
}

std::shared_ptr<Type> Type::createArray(std::shared_ptr<Type> elementType, size_t size) {
  auto type = create(TypeTag::ARRAY);
  type->arr.elementType = std::move(elementType);
  type->arr.size        = size;
  return type;
}

std::shared_ptr<Type> Type::createPointer(std::shared_ptr<Type> pointedType) {
  auto type = create(TypeTag::PTR);
  type->ptr.pointedType = std::move(pointedType);
  return type;
}

std::shared_ptr<Type> Type::createFunction(
  std::shared_ptr<Type>              returnType,
  std::vector<std::shared_ptr<Type>> args,
  bool                               isVariadic
) {
  auto type = create(TypeTag::FUNCTION);
  type->fn.returnType = std::move(returnType);
  type->fn.args       = std::move(args);
  type->fn.isVariadic = isVariadic;
  return type;
}

std::shared_ptr<Type> Type::createLambda(std::shared_ptr<Type> fn, std::vector<std::shared_ptr<Type>> captures) {
  auto type = create(TypeTag::LAMBDA);
  type->lambda.fn       = std::move(fn);
  type->lambda.captures = std::move(captures);
  return type;
}

std::shared_ptr<Type> Type::createEnum(
  std::string            name,
  std::shared_ptr<Type>  base,
  std::vector<EnumField> members
) {
  auto type = create(TypeTag::ENUM);
  type->_enum.name    = std::move(name);
  type->_enum.base    = std::move(base);
  type->_enum.members = std::move(members);
  return type;
}

std::shared_ptr<Type> Type::createTuple(std::vector<std::shared_ptr<Type>> members) {
  auto type = create(TypeTag::TUPLE);
  type->tuple.members = std::move(members);
  return type;
}

std::shared_ptr<Type> Type::createStruct(std::string name, StructMembers members, bool packed) {
  auto type = create(TypeTag::STRUCT);
  type->_struct.name    = std::move(name);
  type->_struct.members = std::move(members);
  type->_struct.packed  = packed;
  return type;
}

std::shared_ptr<Type> Type::inferFromNode(codegen::ModuleContext& ctx, std::shared_ptr<ast::Node> node) {
  return node->generateType(ctx, {});
}

void Type::registerCustomType(const std::string& name, std::shared_ptr<Type> type) {
  customTypes[name] = std::move(type);
}

bool Type::hasCustomType(const std::string& name) {
  return customTypes.contains(name);
}

std::shared_ptr<Type> Type::getCustomType(const std::string& name) {
  if (hasCustomType(name)) {
    return customTypes[name];
  }

  return nullptr;
}

bool Type::isBuiltIn(const std::string& name) {
  return oneOf(name, "void", "bool", "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64", "isize", "usize");
}

std::shared_ptr<Type> Type::alignTypes(std::shared_ptr<Type> lhs, std::shared_ptr<Type> rhs) {
  if (lhs->isVoid() || rhs->isVoid()) return createVoid();
  return (lhs->tag >= rhs->tag) ? std::move(lhs) : std::move(rhs);
}

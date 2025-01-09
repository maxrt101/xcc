#include "xcc/meta/type.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::meta;

Type::Type(TypeTag tag) : tag(tag) {}

bool Type::operator==(Type& rhs) {
  return tag == rhs.tag;
}

bool Type::operator!=(Type& rhs) {
  return tag != rhs.tag;
}

TypeTag Type::getTag() const {
  return tag;
}

std::shared_ptr<Type> Type::getPointedType() const {
  return pointedType;
}

llvm::Type * Type::getLLVMType(codegen::ModuleContext& ctx) const {
  switch (tag) {
    case TypeTag::VOID:
      return llvm::Type::getVoidTy(*ctx.llvm.ctx);

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

    case TypeTag::PTR:
      return pointedType->getLLVMType(ctx)->getPointerTo();

    default:
      throw CodegenException("Unknown type");
  }
}

std::string Type::toString() const {
  switch (tag) {
    case TypeTag::VOID:  return "void";
    case TypeTag::U8:    return "u8";
    case TypeTag::I8:    return "i8";
    case TypeTag::U16:   return "u16";
    case TypeTag::I16:   return "i16";
    case TypeTag::U32:   return "u32";
    case TypeTag::I32:   return "i32";
    case TypeTag::U64:   return "u64";
    case TypeTag::I64:   return "i64";
    case TypeTag::F32:   return "f32";
    case TypeTag::F64:   return "f64";
    case TypeTag::PTR:   return pointedType->toString() + "*";
    default:
      return "<?>";
  }
}

bool Type::isVoid() const {
  return is(TypeTag::VOID);
}

bool Type::isSigned() const {
  return isAnyOf(TypeTag::I8, TypeTag::I16, TypeTag::I32, TypeTag::I64);
}

bool Type::isInteger() const {
  return isAnyOf(TypeTag::I8, TypeTag::I16, TypeTag::I32, TypeTag::I64,
                 TypeTag::U8, TypeTag::U16, TypeTag::U32, TypeTag::U64);
}

bool Type::isFloat() const {
  return isAnyOf(TypeTag::F32, TypeTag::F64);
}

bool Type::isPointer() const {
  return is(TypeTag::PTR);
}

llvm::Value * Type::getDefault(codegen::ModuleContext& ctx) const {
  switch (tag) {
    case TypeTag::U8:
    case TypeTag::U16:
    case TypeTag::U32:
    case TypeTag::U64:
    case TypeTag::I8:
    case TypeTag::I16:
    case TypeTag::I32:
    case TypeTag::I64:
      return llvm::ConstantInt::get(getLLVMType(ctx), 0);

    case TypeTag::F32:
    case TypeTag::F64:
      return llvm::ConstantFP::get(getLLVMType(ctx), 0.0);

    case TypeTag::PTR:
      return llvm::Constant::getNullValue(getLLVMType(ctx));

    case TypeTag::VOID:
    default:
      return nullptr;
  }
}

std::shared_ptr<Type> Type::create(TypeTag tag) {
  return std::make_shared<Type>(tag);
}

std::shared_ptr<Type> Type::fromTypeName(const std::string& name) {
  if (name == "void") {
    return Type::createVoid();
  } else if (name == "i8") {
    return Type::createI8();
  } else if (name == "i16") {
    return Type::createI16();
  } else if (name == "i32") {
    return Type::createI32();
  } else if (name == "i64") {
    return Type::createI64();
  } else if (name == "u8") {
    return Type::createU8();
  } else if (name == "u16") {
    return Type::createU16();
  } else if (name == "u32") {
    return Type::createU32();
  } else if (name == "u64") {
    return Type::createU64();
  } else if (name == "f32") {
    return Type::createF32();
  } else if (name == "f64") {
    return Type::createF64();
  }

  throw CodegenException("Unknown type '" + name + "'");
}

std::shared_ptr<Type> Type::createVoid() {
  return Type::create(TypeTag::VOID);
}

std::shared_ptr<Type> Type::createI8() {
  return Type::create(TypeTag::I8);
}

std::shared_ptr<Type> Type::createI16() {
  return Type::create(TypeTag::I16);
}

std::shared_ptr<Type> Type::createI32() {
  return Type::create(TypeTag::I32);
}

std::shared_ptr<Type> Type::createI64() {
  return Type::create(TypeTag::I64);
}

std::shared_ptr<Type> Type::createU8() {
  return Type::create(TypeTag::U8);
}

std::shared_ptr<Type> Type::createU16() {
  return Type::create(TypeTag::U16);
}

std::shared_ptr<Type> Type::createU32() {
  return Type::create(TypeTag::U32);
}

std::shared_ptr<Type> Type::createU64() {
  return Type::create(TypeTag::U64);
}

std::shared_ptr<Type> Type::createF32() {
  return Type::create(TypeTag::F32);
}

std::shared_ptr<Type> Type::createF64() {
  return Type::create(TypeTag::F64);
}

std::shared_ptr<Type> Type::createPointer(std::shared_ptr<Type> pointedType) {
  auto t = Type::create(TypeTag::PTR);
  t->pointedType = std::move(pointedType);
  return t;
}

std::shared_ptr<Type> Type::alignTypes(std::shared_ptr<Type> lhs, std::shared_ptr<Type> rhs) {
  return (lhs->tag >= rhs->tag) ? std::move(lhs) : std::move(rhs);
}

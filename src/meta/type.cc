#include "xcc/meta/type.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::meta;

std::unordered_map<std::string, std::shared_ptr<Type>> Type::customTypes;

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

    case TypeTag::STRUCT: {
      std::vector<llvm::Type*> elements;

      for (auto & member : members) {
        elements.push_back(member.second->getLLVMType(ctx));
      }

      return llvm::StructType::get(*ctx.llvm.ctx, elements, false);
    }


    default:
      throw CodegenException("Unknown type");
  }
}

std::string Type::toString() const {
  switch (tag) {
    case TypeTag::VOID:   return "void";
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
    case TypeTag::PTR:    return pointedType->toString() + "*";
    case TypeTag::STRUCT: {
      std::string result = "struct {";
      for (size_t i = 0; i < members.size(); ++i) {
        result += members[i].first.c_str();
        result += ": ";
        result += members[i].second->toString();
        if (i + 1 != members.size()) {
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

bool Type::isStruct() const {
  return is(TypeTag::STRUCT);
}

int Type::getNumberBitWidth() const {
  switch (tag) {
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
      return 64;
    case TypeTag::PTR:
    case TypeTag::STRUCT:
    case TypeTag::VOID:
    default:
      return 0;
  }
}

bool Type::hasMember(const std::string& name) const {
  return std::find_if(
    members.begin(), members.end(),
    [&name](auto & element) {
      return element.first == name;
    }
  ) != members.end();
}

size_t Type::getMemberIndex(const std::string& name) const {
  for (size_t idx = 0; idx < members.size(); ++idx) {
    if (members[idx].first == name) {
      return idx;
    }
  }

  throw CodegenException("Struct '" + toString() + "' has no member '" + name + "'");
}

std::shared_ptr<Type> Type::getMemberType(const std::string& name) const {
  for (size_t idx = 0; idx < members.size(); ++idx) {
    if (members[idx].first == name) {
      return members[idx].second;
    }
  }

  throw CodegenException("Struct '" + toString() + "' has no member '" + name + "'");
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

    case TypeTag::STRUCT: {
      // llvm::StructType * llvm_type = (llvm::StructType *) getLLVMType(ctx);

      std::vector<llvm::Constant *> initializers;

      for (auto & member : members) {
        initializers.push_back((llvm::Constant *) member.second->getDefault(ctx));
      }

      return llvm::ConstantStruct::get((llvm::StructType *) getLLVMType(ctx), initializers);
    }

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
  } else {
    if (customTypes.find(name) != customTypes.end()) {
      return customTypes[name];
    }
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

std::shared_ptr<Type> Type::createPointer(std::shared_ptr<Type> pointedType) {
  auto t = Type::create(TypeTag::PTR);
  t->pointedType = std::move(pointedType);
  return t;
}

std::shared_ptr<Type> Type::createStruct(StructMembers members) {
  auto type = create(TypeTag::STRUCT);
  type->members = std::move(members);
  return type;
}

void Type::registerCustomType(const std::string& name, std::shared_ptr<Type> type) {
  customTypes[name] = std::move(type);
}

std::shared_ptr<Type> Type::alignTypes(std::shared_ptr<Type> lhs, std::shared_ptr<Type> rhs) {
  return (lhs->tag >= rhs->tag) ? std::move(lhs) : std::move(rhs);
}

#include "xcc/meta/type.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/util/string.h"

using namespace xcc::meta;

// TODO: Should be scoped per-module (or per-file)!
std::unordered_map<std::string, std::shared_ptr<Type>> Type::customTypes;

Type::Type(TypeTag tag) : tag(tag) {}

bool Type::operator==(Type& rhs) {
  if (tag != rhs.tag) {
    return false;
  }

  if (tag == TypeTag::PTR) {
    return ptr.pointedType == rhs.ptr.pointedType;
  }

  if (tag == TypeTag::STRUCT) {
    return _struct.name == rhs._struct.name;
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
  return ptr.pointedType;
}

std::shared_ptr<Type> Type::getBaseType() {
  if (isPointer()) {
    return ptr.pointedType->getBaseType();
  }

  return shared_from_this();
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

    case TypeTag::ISIZE:
    case TypeTag::USIZE: {
      auto bits = ctx.llvm.module->getDataLayout().getPointerSizeInBits();
      return llvm::IntegerType::get(*ctx.llvm.ctx, bits);
    }

    case TypeTag::PTR:
      return llvm::PointerType::get(ptr.pointedType->getLLVMType(ctx), 0);

    case TypeTag::FUNCTION:
      return llvm::PointerType::getUnqual(getLLVMFunctionType(ctx));

    case TypeTag::STRUCT: {
      std::vector<llvm::Type*> elements;

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
  assertThrow(isFunction(), std::runtime_error("Type is not a function"));

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

std::string Type::getName() const {
  if (tag == TypeTag::STRUCT) {
    return _struct.name;
  }

  return toString();
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
    case TypeTag::ISIZE:  return "isize";
    case TypeTag::USIZE:  return "usize";
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

bool Type::isPointer() const {
  return is(TypeTag::PTR);
}

bool Type::isFunction() const {
  return is(TypeTag::FUNCTION);
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
    // TODO: ISIZE/USIZE
    case TypeTag::PTR:
    case TypeTag::FUNCTION:
    case TypeTag::STRUCT:
    case TypeTag::VOID:
    default:
      return 0;
  }
}

bool Type::hasMember(const std::string& name) const {
  assertThrow(isStruct(), "Type is not a struct");

  return std::find_if(
    _struct.members.begin(), _struct.members.end(),
    [&name](auto & element) {
      return element.first == name;
    }
  ) != _struct.members.end();
}

size_t Type::getMemberIndex(const std::string& name) const {
  assertThrow(isStruct(), "Type is not a struct");

  for (size_t idx = 0; idx < _struct.members.size(); ++idx) {
    if (_struct.members[idx].first == name) {
      return idx;
    }
  }

  Error(ERROR_UNKNOWN_MEMBER, {}, "Struct '" + toString() + "' has no member '" + name + "'").throwException();
}

std::shared_ptr<Type> Type::getMemberType(const std::string& name) const {
  assertThrow(isStruct(), "Type is not a struct");

  for (size_t idx = 0; idx < _struct.members.size(); ++idx) {
    if (_struct.members[idx].first == name) {
      return _struct.members[idx].second;
    }
  }

  Error(ERROR_UNKNOWN_MEMBER, {}, "Struct '" + toString() + "' has no member '" + name + "'").throwException();
}

std::shared_ptr<Type> Type::getReturnType() const {
  assertThrow(isFunction(), "Type is not a function");

  return fn.returnType;
}

size_t Type::getArgumentCount() const {
  assertThrow(isFunction(), "Type is not a function");

  return fn.args.size();
}

std::shared_ptr<Type> Type::getArgumentType(size_t i) const {
  assertThrow(isFunction(), "Type is not a function");
  assertThrow(i < fn.args.size(), "Out of bound argument type request");

  return fn.args[i];
}

bool Type::isVariadic() const {
  return fn.isVariadic;
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

    case TypeTag::ISIZE:
    case TypeTag::USIZE:
      return llvm::ConstantInt::get(getLLVMType(ctx), 0);

    case TypeTag::PTR:
      return llvm::Constant::getNullValue(getLLVMType(ctx));

    case TypeTag::FUNCTION: {
      return llvm::Constant::getNullValue(getLLVMType(ctx));
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

std::shared_ptr<xcc::ast::Node> Type::toAst(SourceSpan span) const {
  switch (tag) {
    case TypeTag::VOID:  return ast::Type::create(span, ast::Identifier::create(span, "void"));
    case TypeTag::U8:    return ast::Type::create(span, ast::Identifier::create(span, "u8"));
    case TypeTag::U16:   return ast::Type::create(span, ast::Identifier::create(span, "u16"));
    case TypeTag::U32:   return ast::Type::create(span, ast::Identifier::create(span, "u32"));
    case TypeTag::U64:   return ast::Type::create(span, ast::Identifier::create(span, "u64"));
    case TypeTag::I8:    return ast::Type::create(span, ast::Identifier::create(span, "i8"));
    case TypeTag::I16:   return ast::Type::create(span, ast::Identifier::create(span, "i16"));
    case TypeTag::I32:   return ast::Type::create(span, ast::Identifier::create(span, "i32"));
    case TypeTag::I64:   return ast::Type::create(span, ast::Identifier::create(span, "i64"));
    case TypeTag::F32:   return ast::Type::create(span, ast::Identifier::create(span, "f32"));
    case TypeTag::F64:   return ast::Type::create(span, ast::Identifier::create(span, "f64"));
    case TypeTag::ISIZE: return ast::Type::create(span, ast::Identifier::create(span, "isize"));
    case TypeTag::USIZE: return ast::Type::create(span, ast::Identifier::create(span, "usize"));
    case TypeTag::PTR:   return ast::Type::create(span, ptr.pointedType->toAst(), true);
    case TypeTag::FUNCTION: {
      std::vector<std::shared_ptr<ast::Node>> args;

      for (auto& arg : fn.args) {
        args.push_back(arg->toAst());
      }

      return ast::Type::createFunction(span, fn.returnType->toAst(), args, fn.isVariadic);
    }

    case TypeTag::STRUCT: {
      return ast::Type::create(span, ast::Identifier::create(span, _struct.name), false);
    }

    default:
      return xcc::ast::Empty::create();
  }
}

std::shared_ptr<Type> Type::create(TypeTag tag) {
  return std::make_shared<Type>(tag);
}

std::shared_ptr<Type> Type::fromTypeName(codegen::GlobalContext& ctx, const std::string& name) {
  switch (util::strhash(name.c_str())) {
    case util::strhash("void"):  return createVoid();
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

      Error(ERROR_UNKNOWN_TYPE, {}, "Unknown type '" + name + "'").throwException();
    }
  }
}

std::shared_ptr<Type> Type::createVoid() {
  return create(TypeTag::VOID);
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

std::shared_ptr<Type> Type::createPointer(std::shared_ptr<Type> pointedType) {
  auto type = Type::create(TypeTag::PTR);
  type->ptr.pointedType = std::move(pointedType);
  return type;
}

std::shared_ptr<Type> Type::createFunction(
  std::shared_ptr<Type>              returnType,
  std::vector<std::shared_ptr<Type>> args,
  bool                               isVariadic
) {
  auto type = create(TypeTag::FUNCTION);
  type->fn.returnType = returnType;
  type->fn.args       = args;
  type->fn.isVariadic = isVariadic;
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

std::shared_ptr<Type> Type::alignTypes(std::shared_ptr<Type> lhs, std::shared_ptr<Type> rhs) {
  return (lhs->tag >= rhs->tag) ? std::move(lhs) : std::move(rhs);
}

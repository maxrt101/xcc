#include "xcc/meta/value.h"
#include <llvm/IR/IRBuilder.h>

using namespace xcc::meta;

TypedValue::TypedValue() : type(nullptr), value(nullptr) {}

TypedValue::TypedValue(std::shared_ptr<xcc::meta::Type> type, llvm::AllocaInst * value)
  : type(std::move(type)), value(value) {}

std::shared_ptr<TypedValue> TypedValue::create(codegen::ModuleContext& ctx, llvm::Function * fn, std::shared_ptr<xcc::meta::Type> type, const std::string& name) {
    llvm::IRBuilder<> tmp(&fn->getEntryBlock(), fn->getEntryBlock().begin());
    return std::make_shared<TypedValue>(std::move(type), tmp.CreateAlloca(type->getLLVMType(ctx), nullptr, name));
}

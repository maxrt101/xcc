#include "xcc/ast/string.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

String::String(const std::string& value) : Node(AST_EXPR_STRING), value(value) {}

std::shared_ptr<String> String::create(const std::string& value) {
  return std::make_shared<String>(value);
}

llvm::Value * String::generateValue(codegen::ModuleContext& ctx) {
  auto hash = std::hash<std::string>{}(value);
  auto name = ".str." + std::to_string(hash);

  llvm::Constant * constant = llvm::ConstantDataArray::getString(*ctx.globalContext.globalModule->llvm.ctx, value, true);

  auto global = new llvm::GlobalVariable(
      *ctx.globalContext.globalModule->llvm.module,
      constant->getType(),
      true,
      llvm::GlobalValue::ExternalLinkage,
      constant,
      name
  );

  auto extern_global = llvm::cast<llvm::GlobalVariable>(
      ctx.llvm.module->getOrInsertGlobal(name, llvm::Type::getInt32Ty(*ctx.llvm.ctx)));

  return ctx.ir_builder->CreateInBoundsGEP(
      extern_global->getValueType(), extern_global,
      {ctx.ir_builder->getInt32(0), ctx.ir_builder->getInt32(0)},
      "str_ptr"
  );
}

std::shared_ptr<xcc::meta::Type> String::generateType(codegen::ModuleContext& ctx) {
  return meta::Type::createPointer(meta::Type::createI8());
}

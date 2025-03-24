#include "xcc/ast/string.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

String::String(std::string value) : Node(AST_EXPR_STRING), value(std::move(value)) {}

std::shared_ptr<String> String::create(std::string value) {
  return std::make_shared<String>(std::move(value));
}

llvm::Value * String::generateValue(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  auto hash = std::hash<std::string>{}(value);
  auto name = ".str." + std::to_string(hash);

  llvm::Constant * constant = llvm::ConstantDataArray::getString(*ctx.globalContext.globalModule->llvm.ctx, value, true);

  if (!constant->isConstantUsed()) {
    [[maybe_unused]] auto global = new llvm::GlobalVariable(
        *ctx.globalContext.globalModule->llvm.module,
        constant->getType(),
        true,
        llvm::GlobalValue::ExternalLinkage,
        constant,
        name
    );
  }

  auto extern_global = llvm::cast<llvm::GlobalVariable>(
      ctx.llvm.module->getOrInsertGlobal(name, llvm::Type::getInt32Ty(*ctx.llvm.ctx)));

  auto zero = ctx.ir_builder->getInt32(0);

  return ctx.ir_builder->CreateInBoundsGEP(
      extern_global->getValueType(), extern_global, {zero, zero}, "str_ptr"
  );
}

llvm::Value * String::generateValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  return llvm::ConstantDataArray::getString(*ctx.globalContext.globalModule->llvm.ctx, value, true);
}

std::shared_ptr<xcc::meta::Type> String::generateType(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  return meta::Type::createPointer(meta::Type::createI8());
}

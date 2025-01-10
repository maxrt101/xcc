#include "xcc/ast/vardecl.h"
#include "xcc/ast/number.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

VarDecl::VarDecl(
  std::shared_ptr<Identifier> name,
  std::shared_ptr<Type> type,
  std::shared_ptr<Node> value,
  bool global
) : Node(AST_VAR_DECL),
    name(std::move(name)),
    type(std::move(type)),
    value(std::move(value)),
    global(global) {}

std::shared_ptr<VarDecl> VarDecl::create(
    std::shared_ptr<Identifier> name,
    std::shared_ptr<Type> type,
    std::shared_ptr<Node> value,
    bool global
) {
  return std::make_shared<VarDecl>(std::move(name), std::move(type), std::move(value), global);
}

llvm::Value * VarDecl::generateValue(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  auto meta_type = type->generateType(ctx, {});

  if (global) {
    // FIXME: Check if can convert to constant

    auto constant = (llvm::Constant *)(value
        ? value->generateValueWithoutLoad(ctx, {Number::Payload::create(meta_type->getNumberBitWidth())})
        : meta_type->getDefault(ctx));

    ctx.globalContext.globals[name->value] = meta_type;

    [[maybe_unused]] auto global = new llvm::GlobalVariable(
        *ctx.globalContext.globalModule->llvm.module,
        constant->getType(),
        false,
        llvm::GlobalValue::ExternalLinkage,
        constant,
        name->value
    );

    auto extern_global = llvm::cast<llvm::GlobalVariable>(
      ctx.llvm.module->getOrInsertGlobal(name->value, llvm::Type::getInt32Ty(*ctx.llvm.ctx)));

    return extern_global;
  } else {
    auto fn = ctx.ir_builder->GetInsertBlock()->getParent();

    llvm::Value * init = value
        ? value->generateValue(ctx, {Number::Payload::create(meta_type->getNumberBitWidth())})
        : meta_type->getDefault(ctx);

    init = codegen::castIfNotSame(ctx, init, meta_type->getLLVMType(ctx));

    auto tv = meta::TypedValue::create(ctx, fn, meta_type, name->value);

    ctx.ir_builder->CreateStore(init, tv->value);

    ctx.locals[name->value] = tv;

    return init;
  }
}

std::shared_ptr<xcc::meta::Type> VarDecl::generateType(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  return type->generateType(ctx, {});
}

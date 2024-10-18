#include "xcc/ast/vardecl.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

VarDecl::VarDecl(std::shared_ptr<Identifier> name, std::shared_ptr<Type> type, std::shared_ptr<Node> value)
    : Node(AST_VAR_DECL), name(std::move(name)), type(std::move(type)), value(std::move(value)) {}

std::shared_ptr<VarDecl> VarDecl::create(std::shared_ptr<Identifier> name, std::shared_ptr<Type> type, std::shared_ptr<Node> value) {
  return std::make_shared<VarDecl>(std::move(name), std::move(type), std::move(value));
}

llvm::Value * VarDecl::generateValue(codegen::ModuleContext& ctx) {
  auto fn = ctx.ir_builder->GetInsertBlock()->getParent();
  auto meta_type = type->generateType(ctx);

  llvm::Value * init = value ? value->generateValue(ctx) : meta_type->getDefault(ctx);

  init = codegen::castIfNotSame(ctx, init, meta_type->getLLVMType(ctx));

  auto tv = meta::TypedValue::create(ctx, fn, meta_type, name->value);

  ctx.ir_builder->CreateStore(init, tv->value);

  ctx.namedValues[name->value] = tv;

  return init;
}

std::shared_ptr<xcc::meta::Type> VarDecl::generateType(codegen::ModuleContext& ctx) {
  return type->generateType(ctx);
}

#include "xcc/ast/for.h"
#include "xcc/exceptions.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

For::For(std::shared_ptr<VarDecl> init, std::shared_ptr<Node> cond, std::shared_ptr<Node> step, std::shared_ptr<Node> body)
  : Node(AST_FOR), init(std::move(init)), cond(std::move(cond)), step(std::move(step)), body(std::move(body)) {}

std::shared_ptr<For> For::create(std::shared_ptr<VarDecl> init, std::shared_ptr<Node> cond, std::shared_ptr<Node> step, std::shared_ptr<Node> body) {
  return std::make_shared<For>(std::move(init), std::move(cond), std::move(step), std::move(body));
}

llvm::Value * For::generateValue(codegen::ModuleContext& ctx) {
  auto fn = ctx.ir_builder->GetInsertBlock()->getParent();

  auto var = meta::TypedValue::create(ctx, fn, init->generateType(ctx), init->name->value);

  auto init_val = init->generateValue(ctx);

  ctx.ir_builder->CreateStore(init_val, var->value);

  auto loop_block = llvm::BasicBlock::Create(*ctx.llvm.ctx, "for_loop", fn);

  ctx.ir_builder->CreateBr(loop_block);

  ctx.ir_builder->SetInsertPoint(loop_block);

  auto old_val = ctx.locals[init->name->value];
  ctx.locals[init->name->value] = var;

  //
  auto body_val = body->generateValue(ctx);

  //
  auto step_val = step->generateValue(ctx);

  //
  auto cond_val = cond->generateValue(ctx);

  auto i64_type = meta::Type::createI64()->getLLVMType(ctx);

  if (!cond_val->getType()->isIntegerTy(64)) {
    cond_val = codegen::cast(ctx, cond_val, i64_type);
  }

  cond_val = ctx.ir_builder->CreateICmpNE(cond_val, llvm::ConstantInt::get(i64_type, 0), "for_cond");

  auto loop_after_block = llvm::BasicBlock::Create(*ctx.llvm.ctx, "after_loop", fn);

  ctx.ir_builder->CreateCondBr(cond_val, loop_block, loop_after_block);

  ctx.ir_builder->SetInsertPoint(loop_after_block);

  if (old_val) {
    ctx.locals[init->name->value] = old_val;
  } else {
    ctx.locals.erase(init->name->value);
  }

  return meta::Type::createI64()->getDefault(ctx);
}

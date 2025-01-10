#include "xcc/ast/if.h"
#include "xcc/ast/return.h"
#include "xcc/exceptions.h"
#include "xcc/codegen.h"
#include "xcc/ast.h"

using namespace xcc::ast;

If::If(std::shared_ptr<Node> condition, std::shared_ptr<Node> then_branch, std::shared_ptr<Node> else_branch)
  : Node(AST_IF), condition(std::move(condition)), then_branch(std::move(then_branch)), else_branch(std::move(else_branch)) {}

std::shared_ptr<If> If::create(std::shared_ptr<Node> condition, std::shared_ptr<Node> then_branch, std::shared_ptr<Node> else_branch) {
  return std::make_shared<If>(std::move(condition), std::move(then_branch), std::move(else_branch));
}

llvm::Value * If::generateValue(codegen::ModuleContext& ctx, void * payload) {
  auto cond_val = throwIfNull(condition->generateValue(ctx), CodegenException("if condition generated NULL"));

  if (!cond_val) {
    throw CodegenException("Error generating condition of 'if' statement");
  }

  auto then_type = throwIfNull(then_branch->generateType(ctx), CodegenException("if then branch generated NULL type"));
  auto else_type = meta::Type::createVoid();

  auto common_type = then_type;

  // If else_branch exists, use its type - otherwise use then_branch type
  if (else_branch) {
    else_type = throwIfNull(else_branch->generateType(ctx), CodegenException("if else branch generated NULL type"));
    common_type = meta::Type::alignTypes(then_type, else_type);
  }

  auto i64_type = meta::Type::createI64()->getLLVMType(ctx);

  if (!cond_val->getType()->isIntegerTy(64)) {
    cond_val = codegen::cast(ctx, cond_val, i64_type);
  }

  cond_val = ctx.ir_builder->CreateICmpNE(cond_val, llvm::ConstantInt::get(i64_type, 0), "ifcond");

  auto fn = ctx.ir_builder->GetInsertBlock()->getParent();

  auto then_block = llvm::BasicBlock::Create(*ctx.llvm.ctx, "then", fn);
  auto else_block = llvm::BasicBlock::Create(*ctx.llvm.ctx, "else");
  auto merge_block = llvm::BasicBlock::Create(*ctx.llvm.ctx, "ifcont");

  ctx.ir_builder->CreateCondBr(cond_val, then_block, else_block);

  // If then branch
  ctx.ir_builder->SetInsertPoint(then_block);
  auto then_val = throwIfNull(then_branch->generateValue(ctx), CodegenException("if then branch generated NULL"));

  if (!then_val) {
    throw CodegenException("Error generating 'then' block of 'if' statement");
  }

  then_val = codegen::castIfNotSame(ctx, then_val, common_type->getLLVMType(ctx));

  if (!isOrIsLastInBlock(then_branch, AST_RETURN)) {
    ctx.ir_builder->CreateBr(merge_block);
  }

  then_block = ctx.ir_builder->GetInsertBlock();

  // If else branch
  fn->insert(fn->end(), else_block);
  ctx.ir_builder->SetInsertPoint(else_block);

  auto else_val = else_branch ? throwIfNull(else_branch->generateValue(ctx), CodegenException("If else branch generated NULL")) : nullptr;

  if (else_val) {
    else_val = codegen::castIfNotSame(ctx, else_val, common_type->getLLVMType(ctx));
    if (!isOrIsLastInBlock(else_branch, AST_RETURN)) {
      ctx.ir_builder->CreateBr(merge_block);
    }
  } else {
    ctx.ir_builder->CreateBr(merge_block);
  }


  else_block = ctx.ir_builder->GetInsertBlock();

  // Merge block (after then & else)
  fn->insert(fn->end(), merge_block);
  ctx.ir_builder->SetInsertPoint(merge_block);
  auto phi = ctx.ir_builder->CreatePHI(common_type->getLLVMType(ctx), 2, "iftmp");

  phi->addIncoming(then_val, then_block);

  if (else_val) {
    phi->addIncoming(else_val, else_block);
  }

  return phi;
}

std::shared_ptr<xcc::meta::Type> If::generateType(codegen::ModuleContext& ctx, void * payload) {
  auto then_type = throwIfNull(then_branch->generateType(ctx), CodegenException("if then branch generated NULL type"));
  auto else_type = meta::Type::createVoid();

  if (else_branch) {
    else_type = throwIfNull(else_branch->generateType(ctx), CodegenException("if else branch generated NULL type"));
  }

  return meta::Type::alignTypes(then_type, else_type);
}

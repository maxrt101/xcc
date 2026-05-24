#include "xcc/ast/if.h"
#include "xcc/ast/return.h"
#include "xcc/exceptions.h"
#include "xcc/codegen.h"
#include "xcc/ast.h"

using namespace xcc::ast;

If::If(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> condition, std::shared_ptr<Node> then_branch, std::shared_ptr<Node> else_branch)
  : Node(AST_IF, span, scope), condition(std::move(condition)), then_branch(std::move(then_branch)), else_branch(std::move(else_branch)) {}

std::shared_ptr<If> If::create(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> condition, std::shared_ptr<Node> then_branch, std::shared_ptr<Node> else_branch) {
  return std::make_shared<If>(span, scope, std::move(condition), std::move(then_branch), std::move(else_branch));
}

std::shared_ptr<Node> If::clone() {
  return withAttrs(create(span, scope, condition->clone(), then_branch->clone(), else_branch ? else_branch->clone() : nullptr));
}

void If::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(globalContext, condition, visitor, ignoreSubtree);
  callVisitor(globalContext, then_branch, visitor, ignoreSubtree);
  callVisitor(globalContext, else_branch, visitor, ignoreSubtree);
}

std::string If::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(indent, newline) +  std::format("if ({}) {}",
    condition->toString(parent, this, indent, newline),
    then_branch->toString(parent, this, indent, newline)
  );

  if (else_branch) {
    res += " else " + else_branch->toString(parent, this, indent, newline);
  }

  return res;
}

llvm::Value * If::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  auto cond_val = raiseIfNull(condition->generateValue(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, condition->span, "Error generating condition of 'if' statement (condition generated NULL)"));

  auto then_type = raiseIfNull(then_branch->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, then_branch->span, "if then branch generated NULL type"));
  auto else_type = meta::Type::createVoid();

  auto common_type = then_type;

  // If else_branch exists, use its type - otherwise use then_branch type
  if (else_branch) {
    else_type = raiseIfNull(else_branch->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, else_branch->span,"if else branch generated NULL type"));
    common_type = meta::Type::alignTypes(then_type, else_type);
  }

  ctx.setDebugLocation(span);
  cond_val = ctx.ir_builder->CreateICmpNE(cond_val, llvm::ConstantInt::get(cond_val->getType(), 0), "ifcond");

  auto fn = ctx.ir_builder->GetInsertBlock()->getParent();

  auto then_block  = llvm::BasicBlock::Create(*ctx.llvm.ctx, "then", fn);
  auto else_block  = llvm::BasicBlock::Create(*ctx.llvm.ctx, "else");
  auto merge_block = llvm::BasicBlock::Create(*ctx.llvm.ctx, "ifcont");

  ctx.ir_builder->CreateCondBr(cond_val, then_block, else_block);

  // If then branch
  ctx.ir_builder->SetInsertPoint(then_block);

  ctx.pushScope(then_branch->span);
  auto then_val = raiseIfNull(then_branch->generateValue(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, then_branch->span, "Error generating 'then' block of 'if' statement (then branch generated NULL)"));
  ctx.popScope();

  if (!common_type->isVoid() && then_val) {
    then_val = codegen::castIfNotSame(ctx, then_val, common_type->getLLVMType(ctx), then_branch->span);
  }

  if (!isOrIsLastInBlock(then_branch, AST_RETURN)) {
    ctx.ir_builder->CreateBr(merge_block);
  }

  then_block = ctx.ir_builder->GetInsertBlock();

  // If else branch
  fn->insert(fn->end(), else_block);
  ctx.ir_builder->SetInsertPoint(else_block);

  llvm::Value * else_val = nullptr;

  if (else_branch) {
    ctx.pushScope(else_branch->span);
    else_val = raiseIfNull(else_branch->generateValue(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, else_branch->span, "Error generating 'then' block of 'if' statement (else branch generated NULL)"));
    ctx.popScope();
  }

  if (else_val) {
    if (!common_type->isVoid()) {
      else_val = codegen::castIfNotSame(ctx, else_val, common_type->getLLVMType(ctx), else_branch->span);
    }

    // Generate branch to merge block if else_branch doesn't return from the function
    if (!isOrIsLastInBlock(else_branch, AST_RETURN)) {
      ctx.ir_builder->CreateBr(merge_block);
    }
  } else {
    // Generate default value for 'else' if else block in AST is empty
    if (!common_type->isVoid()) {
      else_val = common_type->getDefault(ctx);
    }
    ctx.ir_builder->CreateBr(merge_block);
  }

  else_block = ctx.ir_builder->GetInsertBlock();

  // Merge block (after then & else)
  fn->insert(fn->end(), merge_block);
  ctx.ir_builder->SetInsertPoint(merge_block);

  // Generate phi only if common type isn't void, as it's illegal to have void as phi type
  // And it's not required to generate phi if it's not possible for the blocks to converge at
  // merge_block, as they return from the function
  if (!common_type->isVoid()) {
    auto phi = ctx.ir_builder->CreatePHI(common_type->getLLVMType(ctx), 2, "iftmp");

    // Add then block to phy if it doesn't return from the function
    if (!llvm::isa<llvm::ReturnInst>(then_block->getTerminator())) {
      phi->addIncoming(then_val, then_block);
    }

    // Add else block to phy if it doesn't return from the function
    if (!llvm::isa<llvm::ReturnInst>(else_block->getTerminator())) {
      phi->addIncoming(else_val, else_block);
    }

    return phi;
  }

  return nullptr;
}

std::shared_ptr<xcc::meta::Type> If::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  auto then_type = raiseIfNull(then_branch->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, then_branch->span,  "if then branch generated NULL type"));
  auto else_type = meta::Type::createVoid();

  if (else_branch) {
    else_type = raiseIfNull(else_branch->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, else_branch->span, "if else branch generated NULL type"));
  }

  return meta::Type::alignTypes(then_type, else_type);
}

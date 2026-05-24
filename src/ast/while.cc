#include "xcc/ast/while.h"
#include "xcc/exceptions.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

While::While(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> condition, std::shared_ptr<Node> body)
  : Node(AST_WHILE, span, scope), condition(std::move(condition)), body(std::move(body)) {}

std::shared_ptr<While> While::create(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> condition, std::shared_ptr<Node> body) {
  return std::make_shared<While>(span, scope, std::move(condition), std::move(body));
}

std::shared_ptr<Node> While::clone() {
  return withAttrs(create(span, scope, condition->clone(), body->clone()));
}

void While::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(globalContext, condition, visitor, ignoreSubtree);
  callVisitor(globalContext, body, visitor, ignoreSubtree);
}

std::string While::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(indent, newline) + std::format("while ({}) {}",
    condition->toString(parent, this, indent, false),
    body->toString(parent, this, indent, newline)
  );
}

llvm::Value * While::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  auto fn = ctx.ir_builder->GetInsertBlock()->getParent();

  auto cond_block  = llvm::BasicBlock::Create(*ctx.llvm.ctx, "while_cond", fn);
  auto body_block  = llvm::BasicBlock::Create(*ctx.llvm.ctx, "while_body", fn);
  auto after_block = llvm::BasicBlock::Create(*ctx.llvm.ctx, "while_after", fn);

  // Jump from the current block into the condition block
  ctx.ir_builder->CreateBr(cond_block);

  // Condition Block
  ctx.ir_builder->SetInsertPoint(cond_block);

  auto cond_val = condition->generateValue(ctx, payload);

  if (!cond_val->getType()->isIntegerTy(1)) {
    cond_val = ctx.ir_builder->CreateICmpNE(cond_val, llvm::ConstantInt::get(cond_val->getType(), 0), "while_cond_cmp");
  }

  // If true go to body otherwise go to after
  ctx.ir_builder->CreateCondBr(cond_val, body_block, after_block);

  // Body Block
  ctx.ir_builder->SetInsertPoint(body_block);

  ctx.pushScope(span);
  body->generateValue(ctx, payload);
  ctx.popScope();

  // At the end of the body, jump back to the condition
  ctx.ir_builder->CreateBr(cond_block);

  // After Block
  ctx.ir_builder->SetInsertPoint(after_block);

  return meta::Type::createI64()->getDefault(ctx);
}

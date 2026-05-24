#include "xcc/ast/for.h"
#include "xcc/exceptions.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

For::For(SourceSpan span, LexicalScope scope, std::shared_ptr<VarDecl> init, std::shared_ptr<Node> cond, std::shared_ptr<Node> step, std::shared_ptr<Node> body)
  : Node(AST_FOR, span, scope), init(std::move(init)), cond(std::move(cond)), step(std::move(step)), body(std::move(body)) {}

std::shared_ptr<For> For::create(SourceSpan span, LexicalScope scope, std::shared_ptr<VarDecl> init, std::shared_ptr<Node> cond, std::shared_ptr<Node> step, std::shared_ptr<Node> body) {
  return std::make_shared<For>(span, scope, std::move(init), std::move(cond), std::move(step), std::move(body));
}

std::shared_ptr<Node> For::clone() {
  return withAttrs(create(
    span, scope,
    cast<VarDecl>(init->clone()),
    cond->clone(),
    step->clone(),
    body->clone()
  ));
}

void For::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(globalContext, init, visitor, ignoreSubtree);
  callVisitor(globalContext, cond, visitor, ignoreSubtree);
  callVisitor(globalContext, step, visitor, ignoreSubtree);
  callVisitor(globalContext, body, visitor, ignoreSubtree);
}

std::string For::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(indent, newline) + std::format("for ({}; {}; {}) {}",
    init->toString(parent, this, indent, newline),
    cond->toString(parent, this, indent, newline),
    step->toString(parent, this, indent, newline),
    body->toString(parent, this, indent, newline)
  );
}

llvm::Value * For::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  auto fn = ctx.ir_builder->GetInsertBlock()->getParent();

  ctx.pushScope(span);

  auto cond_block  = llvm::BasicBlock::Create(*ctx.llvm.ctx, "for_cond", fn);
  auto body_block  = llvm::BasicBlock::Create(*ctx.llvm.ctx, "for_body", fn);
  auto step_block  = llvm::BasicBlock::Create(*ctx.llvm.ctx, "for_step", fn);
  auto after_block = llvm::BasicBlock::Create(*ctx.llvm.ctx, "for_after", fn);

  // Create iterator variable
  auto var = meta::TypedValue::create(ctx, fn, init->span, init->generateType(ctx, payload), init->name->name());

  // Generate and store initial value for iterator
  auto init_val = init->generateValue(ctx, payload);

  ctx.ir_builder->CreateStore(init_val, var->value);

  ctx.ir_builder->CreateBr(cond_block);

  ctx.ir_builder->SetInsertPoint(cond_block);

  // Condition Block
  auto cond_val = cond->generateValue(ctx, payload);

  cond_val = ctx.ir_builder->CreateICmpNE(cond_val, llvm::ConstantInt::get(cond_val->getType(), 0), "for_cond");

  ctx.ir_builder->CreateCondBr(cond_val, body_block, after_block);

  // Body block
  ctx.ir_builder->SetInsertPoint(body_block);

  body->generateValue(ctx, payload);

  ctx.ir_builder->CreateBr(step_block);

  // Step block
  ctx.ir_builder->SetInsertPoint(step_block);

  // Increment iterator
  step->generateValue(ctx, payload);

  // At the end of the body, jump back to the condition
  ctx.ir_builder->CreateBr(cond_block);

  // After Block
  ctx.ir_builder->SetInsertPoint(after_block);

  ctx.popScope();

  return meta::Type::createI64()->getDefault(ctx);
}

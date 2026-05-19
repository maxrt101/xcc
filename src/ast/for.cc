#include "xcc/ast/for.h"
#include "xcc/exceptions.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

For::For(SourceSpan span, std::shared_ptr<VarDecl> init, std::shared_ptr<Node> cond, std::shared_ptr<Node> step, std::shared_ptr<Node> body)
  : Node(AST_FOR, span), init(std::move(init)), cond(std::move(cond)), step(std::move(step)), body(std::move(body)) {}

std::shared_ptr<For> For::create(SourceSpan span, std::shared_ptr<VarDecl> init, std::shared_ptr<Node> cond, std::shared_ptr<Node> step, std::shared_ptr<Node> body) {
  return std::make_shared<For>(span, std::move(init), std::move(cond), std::move(step), std::move(body));
}

std::shared_ptr<Node> For::clone() {
  return withAttrs(create(
    span,
    cast<VarDecl>(init->clone()),
    cond->clone(),
    step->clone(),
    body->clone()
  ));
}

void For::visit(std::unique_ptr<codegen::GlobalContext>& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
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

  auto var = meta::TypedValue::create(ctx, fn, init->span, init->generateType(ctx, payload), init->name->name());

  auto init_val = init->generateValue(ctx, payload);

  ctx.ir_builder->CreateStore(init_val, var->value);

  auto loop_block = llvm::BasicBlock::Create(*ctx.llvm.ctx, "for_loop", fn);

  ctx.ir_builder->CreateBr(loop_block);

  ctx.ir_builder->SetInsertPoint(loop_block);

  //
  auto body_val = body->generateValue(ctx, payload);

  //
  auto step_val = step->generateValue(ctx, payload);

  //
  auto cond_val = cond->generateValue(ctx, payload);

  auto i1_type = meta::Type::createBool()->getLLVMType(ctx);

  if (!cond_val->getType()->isIntegerTy(1)) {
    cond_val = codegen::cast(ctx, cond_val, i1_type, cond->span);
  }

  cond_val = ctx.ir_builder->CreateICmpNE(cond_val, llvm::ConstantInt::get(i1_type, 0), "for_cond");

  auto loop_after_block = llvm::BasicBlock::Create(*ctx.llvm.ctx, "after_loop", fn);

  ctx.ir_builder->CreateCondBr(cond_val, loop_block, loop_after_block);

  ctx.ir_builder->SetInsertPoint(loop_after_block);

  ctx.popScope();

  return meta::Type::createI64()->getDefault(ctx);
}

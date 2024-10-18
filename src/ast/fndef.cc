#include "xcc/ast/fndef.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc;
using namespace xcc::ast;

FnDef::FnDef(std::shared_ptr<FnDecl> decl, std::shared_ptr<Block> body)
  : Node(AST_FUNCTION_DEF), decl(std::move(decl)), body(std::move(body)) {}

std::shared_ptr<FnDef> FnDef::create(std::shared_ptr<FnDecl> decl, std::shared_ptr<Block> body) {
  return std::make_shared<FnDef>(std::move(decl), std::move(body));
}

llvm::Function * FnDef::generateFunction(codegen::ModuleContext& ctx) {
  decl->generateFunction(ctx);

  auto meta_fn = ctx.globalContext.getMetaFunction(decl->name->value);

  auto fn = ctx.getFunction(decl->name->value);

  if (!fn) {
    throw CodegenException("Error generating Function object for '" + decl->name->value + "'");
  }

  auto basic_block = llvm::BasicBlock::Create(*ctx.llvm.ctx, "entry", fn);

  ctx.ir_builder->SetInsertPoint(basic_block);

  ctx.namedValues.clear();

  for (auto& arg : fn->args()) {
    auto arg_name = std::string(arg.getName());
    ctx.namedValues[arg_name] = meta::TypedValue::create(ctx, fn, meta_fn->args[arg_name], arg_name);
    ctx.ir_builder->CreateStore(&arg, ctx.namedValues[arg_name]->value);
  }

  ctx.globalContext.setCurrentFunction(decl->name->value);

  auto last_val = body->generateValue(ctx);

  if (!body->body.back()->is(AST_RETURN)) {
    last_val = codegen::castIfNotSame(ctx, last_val, meta_fn->getLLVMReturnType(ctx));
    ctx.ir_builder->CreateRet(last_val);
  }

  ctx.globalContext.clearCurrentFunction();

  if (!llvm::verifyFunction(*fn)) {
    throw CodegenException("Function '" + decl->name->value + "' didn't pass validation");
  }

  return fn;
}
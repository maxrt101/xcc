#include "xcc/ast/fndef.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/util/log.h"

using namespace xcc;
using namespace xcc::ast;

static auto logger = xcc::util::log::Logger("FN", util::log::Flag::SPLIT_ON_NEWLINE);

FnDef::FnDef(std::shared_ptr<FnDecl> decl, std::shared_ptr<Block> body)
  : Node(AST_FUNCTION_DEF), decl(std::move(decl)), body(std::move(body)) {}

std::shared_ptr<FnDef> FnDef::create(std::shared_ptr<FnDecl> decl, std::shared_ptr<Block> body) {
  return std::make_shared<FnDef>(std::move(decl), std::move(body));
}

llvm::Function * FnDef::generateFunction(codegen::ModuleContext& ctx, PayloadList payload) {
  decl->generateFunction(ctx, {});

  auto meta_fn = ctx.globalContext.getMetaFunction(decl->name->value);

  auto fn = ctx.getFunction(decl->name->value);

  if (!fn) {
    throw CodegenException("Error generating Function object for '" + decl->name->value + "'");
  }

  auto basic_block = llvm::BasicBlock::Create(*ctx.llvm.ctx, "entry", fn);

  ctx.ir_builder->SetInsertPoint(basic_block);

  ctx.locals.clear();

  for (auto& arg : fn->args()) {
    auto arg_name = std::string(arg.getName());
    ctx.locals[arg_name] = meta::TypedValue::create(ctx, fn, meta_fn->args[arg_name], arg_name);
    ctx.ir_builder->CreateStore(&arg, ctx.locals[arg_name]->value);
  }

  ctx.globalContext.setCurrentFunction(decl->name->value);

  auto last_val = body->generateValue(ctx, {});

  if (!body->body.back()->is(AST_RETURN)) {
    if (meta_fn->returnType->isVoid()) {
      ctx.ir_builder->CreateRetVoid();
    } else {
      last_val = codegen::castIfNotSame(ctx, last_val, meta_fn->getLLVMReturnType(ctx));
      ctx.ir_builder->CreateRet(last_val);
    }
  }

  ctx.globalContext.clearCurrentFunction();

  util::RawStreamCollector collector;
  if (llvm::verifyFunction(*fn, collector.stream())) {
#if USE_PRINT_LLVM_IR_ON_VERIFY_FAIL
    util::RawStreamCollector fn_collector;
    fn->print(*fn_collector.stream());
    logger.debug("Function {} IR:", meta_fn->name);
    logger.print("{}", fn_collector.string());
#endif
    throw CodegenException("Function '" + decl->name->value + "' didn't pass validation\n" + collector.string());
  }

  return fn;
}
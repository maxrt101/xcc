#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/util/llvm.h"
#include "xcc/ast.h"

using namespace xcc;
using namespace xcc::codegen;

constexpr char ANONYMOUS_EXPR_FN_NAME[] = "__anonymous__";


GlobalContext::GlobalContext() {
  jit = JIT::create();
}

std::unique_ptr<GlobalContext> GlobalContext::create() {
  return std::make_unique<GlobalContext>();
}

std::unique_ptr<ModuleContext> GlobalContext::createModule(const std::string& name) {
  return ModuleContext::create(*this, name);
}

void GlobalContext::addModule(std::unique_ptr<ModuleContext>& module) {
  CodegenException::throwIfError(jit->addModule(llvm::orc::ThreadSafeModule(std::move(module->llvm.module), std::move(module->llvm.ctx))));
}

void GlobalContext::addFunction(const std::string& name, std::shared_ptr<meta::Function> fn) {
  functions[name] = std::move(fn);
}

std::shared_ptr<meta::Function> GlobalContext::getMetaFunction(const std::string& name) {
  if (functions.find(name) != functions.end()) {
    return functions[name];
  }

  return nullptr;
}

void GlobalContext::setCurrentFunction(const std::string& name) {
  current_function = name;
}

void GlobalContext::clearCurrentFunction() {
  current_function = "";
}

std::shared_ptr<meta::Function> GlobalContext::getCurrentFunction() {
  return current_function.empty() ? nullptr : functions[current_function];
}

void GlobalContext::runExpr(std::shared_ptr<ast::Node> expr) {
  auto ctx = createModule();

  std::shared_ptr<ast::Block> body;

  if (expr->is(ast::AST_BLOCK)) {
    auto& last = expr->as<ast::Block>()->body.back();
    if (!last->is(ast::AST_RETURN)) {
      last = ast::Return::create(last);
    }
    body = ast::Node::cast<ast::Block>(expr);
  } else {
    body = ast::Block::create({
      expr->is(ast::AST_RETURN) ? expr : ast::Return::create(expr)
    });
  }

  auto type = expr->generateType(*ctx);

  if (!type) {
    // TODO: Logger
    printf("Warning: Can't infer %s return type, resorting to i32\n", ANONYMOUS_EXPR_FN_NAME);
    type = meta::Type::createI32();
  }

  auto fndecl = ast::FnDecl::create(
      ast::Identifier::create(ANONYMOUS_EXPR_FN_NAME),
      ast::Type::create(ast::Identifier::create(type->toString())) // TODO: Fix
  );

  auto fndef = ast::FnDef::create(fndecl, body);

  auto fn = fndef->generateFunction(*ctx);

#if USE_PRINT_LLVM_IR
  fn->print(llvm::outs());
#endif

  auto rt = jit->getMainJitDylib().createResourceTracker();
  auto tsm = llvm::orc::ThreadSafeModule(std::move(ctx->llvm.module), std::move(ctx->llvm.ctx));

  CodegenException::throwIfError(jit->addModule(std::move(tsm), rt));

  auto symbol = jit->lookup(ANONYMOUS_EXPR_FN_NAME);

  if (!symbol) {
    // TODO: Logger
    printf("Can't find '%s'\n", ANONYMOUS_EXPR_FN_NAME);
    return;
  }

  auto result = util::call(type, symbol.get());

#if USE_PRINT_EXPR_RESULT
  switch (result.tag) {
    case util::GenericValueContainer::SIGNED_INTEGER:
      printf("Result: %lld\n", result.value.signed_integer);
      break;

    case util::GenericValueContainer::UNSIGNED_INTEGER:
      printf("Result: %llu\n", result.value.unsigned_integer);
      break;

    case util::GenericValueContainer::FLOATING:
      printf("Result: %g\n", result.value.floating);
      break;

    default:
      break;
  }
#endif

  CodegenException::throwIfError(rt->remove());
}

ModuleContext::ModuleContext(GlobalContext& global, const std::string& name) : globalContext(global) {
  llvm.ctx = std::make_unique<llvm::LLVMContext>();
  llvm.module = std::make_unique<llvm::Module>(name, *llvm.ctx);

  ir_builder = std::make_unique<llvm::IRBuilder<>>(*llvm.ctx);

#if USE_OPTIMIZATION
  opt.fpm = std::make_unique<llvm::FunctionPassManager>();
  opt.lam = std::make_unique<llvm::LoopAnalysisManager>();
  opt.fam = std::make_unique<llvm::FunctionAnalysisManager>();
  opt.cgam = std::make_unique<llvm::CGSCCAnalysisManager>();
  opt.mam = std::make_unique<llvm::ModuleAnalysisManager>();
  opt.pic = std::make_unique<llvm::PassInstrumentationCallbacks>();
  opt.si = std::make_unique<llvm::StandardInstrumentations>(*llvm.ctx, true);

  opt.si->registerCallbacks(*opt.pic, opt.mam.get());

  opt.fpm->addPass(llvm::InstCombinePass());
  opt.fpm->addPass(llvm::ReassociatePass());
  opt.fpm->addPass(llvm::GVNPass());
  opt.fpm->addPass(llvm::SimplifyCFGPass());

  llvm::PassBuilder pass_builder;

  pass_builder.registerModuleAnalyses(*opt.mam);
  pass_builder.registerFunctionAnalyses(*opt.fam);
  pass_builder.crossRegisterProxies(*opt.lam, *opt.fam, *opt.cgam, *opt.mam);
#endif
}

std::unique_ptr<ModuleContext> ModuleContext::create(GlobalContext& global, const std::string& name) {
  return std::make_unique<ModuleContext>(global, name);
}

llvm::Function * ModuleContext::getFunction(const std::string& name) {
  if (auto * fn = llvm.module->getFunction(name)) {
    return fn;
  }

  if (globalContext.functions.find(name) != globalContext.functions.end()) {
    return globalContext.functions[name]->decl->generateFunction(*this);
  }

  return nullptr;
}

llvm::Value * xcc::codegen::cast(ModuleContext& ctx, llvm::Value * val, llvm::Type * target_type) {
  if (!val || !target_type) {
    throw CodegenException("codegen::cast received nullptr");
  }

  if (util::isInteger(val->getType()) && util::isFloatOrDouble(target_type)) {
    return ctx.ir_builder->CreateSIToFP(val, target_type);
  }

  if (util::isFloatOrDouble(val->getType()) && util::isInteger(target_type)) {
    return ctx.ir_builder->CreateFPToSI(val, target_type);
  }

  if (util::isFloatOrDouble(val->getType()) && util::isFloatOrDouble(target_type)) {
    return ctx.ir_builder->CreateFPCast(val, target_type);
  }

  if (util::isInteger(val->getType()) && util::isInteger(target_type)) {
    if (val->getType()->getIntegerBitWidth() > target_type->getIntegerBitWidth()) {
      return ctx.ir_builder->CreateTruncOrBitCast(val, target_type);
    } else {
      return ctx.ir_builder->CreateZExtOrBitCast(val, target_type);
    }
  }

  if (util::isPointer(val->getType()) && util::isInteger(target_type)) {
    return ctx.ir_builder->CreatePtrToInt(val, target_type);
  }

  if (util::isInteger(val->getType()) && util::isPointer(target_type)) {
    return ctx.ir_builder->CreateIntToPtr(val, target_type);
  }

  if (util::isPointer(val->getType()) && util::isPointer(target_type)) {
    return ctx.ir_builder->CreatePointerCast(val, target_type);
  }

  throw CodegenException("Can't perform cast");
}

#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/util/log.h"
#include "xcc/util/llvm.h"
#include "xcc/ast.h"

using namespace xcc;
using namespace xcc::codegen;

constexpr char ANONYMOUS_EXPR_FN_NAME[] = "__anonymous__";

static auto logger = xcc::util::log::Logger("CODEGEN");

GlobalContext::GlobalContext() {
  jit = JIT::create();

  globalModule = ModuleContext::create(*this, "<global>");
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

bool GlobalContext::hasGlobal(const std::string& name) {
  return globals.find(name) != globals.end();
}

llvm::GlobalVariable * GlobalContext::getGlobal(ModuleContext& ctx, const std::string& name) {
  return llvm::cast<llvm::GlobalVariable>(
      ctx.llvm.module->getOrInsertGlobal(name, llvm::Type::getInt32Ty(*ctx.llvm.ctx)));
}

std::shared_ptr<meta::Type> GlobalContext::getGlobalType(const std::string& name) {
  if (!hasGlobal(name)) {
    throw CodegenException("Unknown global variable '" + name + "'");
  }
  return globals[name];
}

void GlobalContext::runExpr(std::shared_ptr<ast::Node> expr) {
  if (!globalModule->llvm.ctx || !globalModule->llvm.module) {
    globalModule = ModuleContext::create(*this, "<global>");
  }

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

  auto type = expr->generateType(*globalModule, {});

  if (!type) {
    logger.warn("Warning: Can't infer {} return type, resorting to i32", ANONYMOUS_EXPR_FN_NAME);
    type = meta::Type::createI32();
  }

  auto fndecl = ast::FnDecl::create(
      ast::Identifier::create(ANONYMOUS_EXPR_FN_NAME),
      ast::Type::create(ast::Identifier::create(type->toString())) // TODO: Fix
  );

  auto fndef = ast::FnDef::create(fndecl, body);

  auto fn = fndef->generateFunction(*globalModule, {});

#if USE_PRINT_LLVM_IR
  util::RawStreamCollector collector;
  fn->print(*collector.stream());
  for (auto &global : globalModule->llvm.module->globals()) {
    global.print(*collector.stream());
    *collector.stream() << "\n";
  }
  logger.debug("IR:\n{}", collector.string());
#endif

  return runFunction(ANONYMOUS_EXPR_FN_NAME);
}

void GlobalContext::runFunction(const std::string& name) {
  auto fn = getMetaFunction(name);

  assertThrow(fn.get(), CodegenException(std::format("Can't find meta-function '{}'", name)));

  auto type = getMetaFunction(name)->returnType;

  auto rt = jit->getMainJitDylib().createResourceTracker();
  auto tsm = llvm::orc::ThreadSafeModule(std::move(globalModule->llvm.module), std::move(globalModule->llvm.ctx));

  CodegenException::throwIfError(jit->addModule(std::move(tsm), rt));

#if USE_DUMP_JIT
  jit->dump();
#endif

  auto symbol = jit->lookup(name);

  assertThrow(bool(symbol), CodegenException(std::format("Can't find symbol '{}'", name)));

  auto result = util::call(type, symbol.get());

#if USE_PRINT_EXPR_RESULT
  switch (result.tag) {
    case util::GenericValueContainer::SIGNED_INTEGER:
      logger.debug("Result: {}", result.value.signed_integer);
      break;

    case util::GenericValueContainer::UNSIGNED_INTEGER:
      logger.debug("Result: {}", result.value.unsigned_integer);
      break;

    case util::GenericValueContainer::FLOATING:
      logger.debug("Result: {}", result.value.floating);
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

  llvm.module->setDataLayout(global.jit->getDataLayout());

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
    return globalContext.functions[name]->decl->generateFunction(*this, {});
  }

  return nullptr;
}

bool ModuleContext::hasLocal(const std::string& name) {
  return locals.find(name) != locals.end();
}

llvm::AllocaInst * ModuleContext::getLocalValue(const std::string& name) {
  return locals[name]->value;
}

std::shared_ptr<meta::Type> ModuleContext::getLocalType(const std::string& name) {
  return locals[name]->type;
}

llvm::Value * xcc::codegen::cast(ModuleContext& ctx, llvm::Value * val, llvm::Type * target_type) {
  if (!val || !target_type) {
    throw CodegenException("codegen::cast received nullptr");
  }

  // TODO: "Can't perform cast" can mean that invalid action is performed on a variable (e.g. struct s {...}; var x: s; x += 1;)
#if 0
  util::RawStreamCollector val_collector;
  val->print(*val_collector.stream());

  util::RawStreamCollector val_type_collector;
  val->getType()->print(*val_type_collector.stream());

  util::RawStreamCollector type_collector;
  target_type->print(*type_collector.stream());

  logger.debug("cast: val=({} {})",
    (void *) val, val_collector.string());

  logger.debug("cast: val_type=({} {})",
    (void *) val->getType(), val_type_collector.string());

  logger.debug("cast: target_type=({} {}) {}",
    (void *) target_type, type_collector.string(), target_type->isPointerTy());
#endif

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

  // TODO: Test if this shit works or even needed
  if (util::isArray(val->getType()) && util::isPointer(target_type)) {
    llvm::Value * zero = ctx.ir_builder->getInt32(0);
    return ctx.ir_builder->CreateInBoundsGEP(
        val->getType(),
        val,
        {zero, zero}
    );
  }

  throw CodegenException("Can't perform cast");
}

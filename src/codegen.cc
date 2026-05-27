#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/util/log.h"
#include "xcc/util/llvm.h"
#include "xcc/util/util.h"
#include "xcc/ast.h"

#include "llvm/Linker/Linker.h"

#include <utility>

#include "xcc/util/fs.h"

using namespace xcc;
using namespace xcc::codegen;

constexpr char ANONYMOUS_EXPR_FN_NAME[] = "__anonymous__";

static auto& logger       = xcc::log::Logger::get("CODEGEN");
static auto& alias_logger = xcc::log::Logger::get("ALIAS");
static auto& ir_logger    = xcc::log::Logger::get("IR");

std::unordered_map<std::string, std::shared_ptr<ast::Node>> GenericsCache::cache;

bool GenericsCache::has(const std::string& name) {
  return cache.contains(name);
}

std::shared_ptr<ast::Node> GenericsCache::get(const std::string& name) {
  if (has(name)) {
    return cache[name];
  }

  return nullptr;
}

void GenericsCache::add(const std::string& name, std::shared_ptr<ast::Node> generic) {
  cache[name] = generic;
}

GlobalContext::GlobalContext(util::Target target, FileId file) : target(target) {
  jit = JIT::create();

  tsc = std::make_unique<llvm::LLVMContext>();

  globalModule = ModuleContext::create(*this, "<global>", file, &target);

  registerBuiltinMacros(*this);
}

std::unique_ptr<GlobalContext> GlobalContext::create(util::Target target, FileId file) {
  return std::make_unique<GlobalContext>(std::move(target), file);
}

std::unique_ptr<ModuleContext> GlobalContext::createModule(const std::string& name, FileId file) {
  return ModuleContext::create(*this, name, file);
}

void GlobalContext::addModule(std::unique_ptr<ModuleContext>& module) {
  module->di_builder->finalize();
  pendingModules.push_back(std::move(module));
}

void GlobalContext::flushModulesToJIT() {
  for (auto& mod : pendingModules) {
    mod->di_builder.reset();
    auto tsm = llvm::orc::ThreadSafeModule(std::move(mod->llvm.module), tsc);
    checkLLVMError(jit->addModule(std::move(tsm)));
  }

  pendingModules.clear();
}

void GlobalContext::mergeModules() const {
  for (auto& mod : pendingModules) {
    mod->di_builder.reset();
    if (llvm::Linker::linkModules(*globalModule->llvm.module, std::move(mod->llvm.module))) {
      logger.error("Failed to link modules <global> and {}", mod->name);
      throw std::runtime_error("Failed to link modules");
    }
  }

  // TODO: ?
  // pendingModules.clear();
}

void GlobalContext::addFunction(const std::string& name, std::shared_ptr<meta::Function> fn, std::shared_ptr<meta::Type> type) {
  functions[name] = {std::move(fn), std::move(type)};
}

std::shared_ptr<meta::Function> GlobalContext::getMetaFunction(const std::string& name) {
  if (functions.find(name) != functions.end()) {
    return functions[name].meta_fn;
  }

  return nullptr;
}

std::shared_ptr<meta::Type> GlobalContext::getMetaFunctionType(const std::string& name) {
  if (functions.find(name) != functions.end()) {
    return functions[name].meta_type;
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
  return current_function.empty() ? nullptr : functions[current_function].meta_fn;
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
    Error(ERROR_UNKNOWN_GLOBAL_VARIABLE, {}, "'{}'", name).raise();
  }
  return globals[name];
}

void GlobalContext::pushModule(const std::string& name, const std::string& path) {
  moduleStack.emplace_back(name, path);
}

void GlobalContext::popModule() {
  moduleStack.pop_back();
}

std::string GlobalContext::getModulePrefix() const {
  std::string res;

  for (auto& mod : moduleStack) {
    res += mod.first + "_";
  }

  return res;
}

std::string GlobalContext::getParentModulePrefix() const {
  std::string res;

  for (size_t i = 0; i < moduleStack.size() - 1; ++i) {
    res += moduleStack[i].first + "_";
  }

  return res;
}

void GlobalContext::registerMacro(const std::string& name, std::shared_ptr<ast::Macro> macro) {
  macros[name] = macro;
}

std::shared_ptr<ast::Macro> GlobalContext::getMacro(const std::string& name) const {
  if (macros.contains(name)) {
    return macros.at(name);
  }

  return nullptr;
}

void GlobalContext::addAlias(const std::string& name, const std::string& value, SourceSpan span) {
  if (aliases.contains(name)) {
    if (aliases[name] == value) {
      return;
    }

    Error(ERROR_ALIAS_EXISTS, span, "'{}'", name).raise();
  }

  alias_logger.debug("Adding alias '{}' -> '{}'", name, value);
  aliases[name] = value;
}

std::string GlobalContext::aliased(const std::string& name) {
  std::string res = name;

  while (aliases.contains(res)) {
    res = aliases[res];
  }

  return res;
}

bool GlobalContext::isAliased(const std::string& name) {
  return aliases.contains(name);
}

void GlobalContext::addConst(const std::string& name, std::shared_ptr<ast::ConstDecl> constant) {
  consts[name] = std::move(constant);
}

std::shared_ptr<ast::ConstDecl> GlobalContext::getConst(const std::string& name) const {
  if (consts.contains(name)) {
    return consts.at(name);
  }

  return nullptr;
}

void GlobalContext::runExpr(std::shared_ptr<ast::Node> expr) {
  if (!globalModule->llvm.ctx || !globalModule->llvm.module) {
    globalModule = ModuleContext::create(*this, "<global>", 0);
  }

  std::shared_ptr<ast::Block> body;

  if (expr->is(ast::AST_BLOCK)) {
    auto& last = expr->as<ast::Block>()->body.back();
    if (!last->is(ast::AST_RETURN)) {
      last = ast::Return::create(last->span, last->scope, last);
    }
    body = ast::Node::cast<ast::Block>(expr);
  } else {
    body = ast::Block::create(expr->span, expr->scope, {
      expr->is(ast::AST_RETURN) ? expr : ast::Return::create(expr->span, expr->scope, expr)
    });
  }

  auto type = expr->generateType(*globalModule, {});

  if (!type) {
    logger.warn("Warning: Can't infer {} return type, resorting to i32", ANONYMOUS_EXPR_FN_NAME);
    type = meta::Type::createI32();
  }

  auto fndecl = ast::FnDecl::create(SourceSpan::builtin(), {},
      ast::Identifier::create(SourceSpan::builtin(), {}, ANONYMOUS_EXPR_FN_NAME),
      ast::Type::create(SourceSpan::builtin(), {}, ast::Identifier::create(SourceSpan::builtin(), {}, type->toString())) // TODO: Fix
  );

  auto fndef = ast::FnDef::create(SourceSpan::builtin(), {}, fndecl, body);

  auto fn = fndef->generateFunction(*globalModule, {});

  if (ir_logger.isEnabled()) {
    util::RawStreamCollector collector;
    fn->print(*collector.stream());
    for (auto &global : globalModule->llvm.module->globals()) {
      global.print(*collector.stream());
      *collector.stream() << "\n";
    }
    ir_logger.debug("IR:\n{}", collector.string());
  }


  return runFunction(ANONYMOUS_EXPR_FN_NAME);
}

void GlobalContext::runFunction(const std::string& name) {
  auto fn = getMetaFunction(name);

  assertRaise(fn.get(), Error(ERROR_UNKNOWN_FUNCTION, {}, "'{}'", name));

  auto type = getMetaFunction(name)->returnType;

  auto rt = jit->getMainJitDylib().createResourceTracker();
  auto tsm = llvm::orc::ThreadSafeModule(std::move(globalModule->llvm.module), tsc);

  checkLLVMError(jit->addModule(std::move(tsm), rt));

#if USE_DUMP_JIT
  jit->dump();
#endif

  auto symbol = jit->lookup(name);

  assertRaise(bool(symbol), Error(ERROR_UNKNOWN_SYMBOL, {}, "'{}'", name));

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

  checkLLVMError(rt->remove());
}

void RAIIContext::addTemporary(llvm::AllocaInst * alloca, std::shared_ptr<meta::Type> type) {
  temporaries.emplace_back(alloca, type);
}

void RAIIContext::forgetLastTemporary() {
  temporaries.pop_back();
}

void RAIIContext::clearTemporaries(ModuleContext& ctx) {
  for (auto& [alloca, type] : temporaries) {
    llvm::Function * drop_fn = ctx.getFunction(type->getDropMethodName());

    if (drop_fn) {
      ctx.ir_builder->CreateCall(drop_fn, {alloca});
    } else {
      Warning(WARNING_STRUCT_DROP_NO_FN, {}, "'{}'", type->getName()).emit();
    }
  }

  temporaries.clear();
}

void RAIIContext::forget(const std::string& name) {
  forgotten.push_back(name);
}

bool RAIIContext::isForgotten(const std::string& name) {
  for (auto& f : forgotten) {
    if (f == name) {
      return true;
    }
  }

  return false;
}

ModuleContext::PhantomScope::PhantomScope(ModuleContext& module, const PhantomsList& vars) : module(module) {
  module.phantomScopes.emplace_back();

  for (auto& [name, type] : vars) {
    module.phantomScopes.back()[name] = type;
  }
}

void Scope::clear(ModuleContext& ctx, bool force) {
  if (!force && cleared) return;

  auto lifetime_end = llvm::Intrinsic::getOrInsertDeclaration(ctx.llvm.module.get(), llvm::Intrinsic::lifetime_end, {ctx.ir_builder->getPtrTy()});

  auto dl = ctx.llvm.module->getDataLayout();

  for (auto it = locals.rbegin(); it != locals.rend(); ++it) {
    auto& tv = locals[*it];

    if (tv->type->isStruct() && tv->type->isDrop() && !raii.isForgotten(*it)) {
      llvm::Function * drop_fn = ctx.getFunction(tv->type->getDropMethodName());

      if (drop_fn) {
        ctx.ir_builder->CreateCall(drop_fn, {tv->value});
      } else {
        Warning(WARNING_STRUCT_DROP_NO_FN, {}, "'{}'", tv->type->getName()).emit();
      }
    }

    if (!XCC_VECTOR_CONTAINS(forgotten, *it)) {
      auto size = dl.getTypeAllocSize(tv->type->getLLVMType(ctx));
      ctx.ir_builder->CreateCall(lifetime_end, {ctx.ir_builder->getInt64(size), tv->value});
    }
  }

  if (!force) {
    cleared = true;
  }
}

ModuleContext::PhantomScope::~PhantomScope() {
  module.phantomScopes.pop_back();
}

void ModuleContext::PhantomScope::add(const std::string& name, std::shared_ptr<meta::Type> type) {
  module.phantomScopes.back()[name] = std::move(type);
}

ModuleContext::ModuleContext(GlobalContext& global, const std::string& name, FileId file, util::Target * target) : name(name), globalContext(global) {
  llvm.ctx = globalContext.tsc.getContext();
  llvm.module = std::make_unique<llvm::Module>(name, *llvm.ctx);

  llvm.module->setDataLayout(  target ? target->machine->createDataLayout() : globalContext.globalModule->llvm.module->getDataLayout());
  llvm.module->setTargetTriple(target ? target->target_triple               : globalContext.globalModule->llvm.module->getTargetTriple());

  ir_builder = std::make_unique<llvm::IRBuilder<>>(*llvm.ctx);

  di_builder = std::make_unique<llvm::DIBuilder>(*llvm.module);

  createCompileUnit(file);

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

std::unique_ptr<ModuleContext> ModuleContext::create(GlobalContext& global, const std::string& name, FileId file,  util::Target * target) {
  return std::make_unique<ModuleContext>(global, name, file, target);
}

void ModuleContext::createCompileUnit(FileId fileId) {
  if (di_compile_unit) return;

  auto file = FileManager::get(fileId);

  di_file = di_builder->createFile(fs::path::getFileName(file->path), fs::path::getParent(file->path));

  di_compile_unit = di_builder->createCompileUnit(
    llvm::dwarf::DW_LANG_C,
    di_file,
    "XCC",
    false, "", 0 // what
  );
}

llvm::DIFile * ModuleContext::getCurrentDIFile() {
  if (!globalContext.moduleStack.empty() && !globalContext.moduleStack.back().second.empty()) {
    auto& record = globalContext.moduleStack.back();
    auto& mod = ModuleCache::get(record.second);

    if (!mod.di_file) {
      mod.di_file = di_builder->createFile(fs::path::getFileName(record.first), fs::path::getParent(record.first));
    }

    return mod.di_file;
  }

  return di_file;
}

llvm::Function * ModuleContext::getFunction(const std::string& name) {
  if (auto * fn = llvm.module->getFunction(name)) {
    return fn;
  }

  if (globalContext.functions.find(name) != globalContext.functions.end()) {
    return globalContext.functions[name].meta_fn->generateFunction(*this);
  }

  return nullptr;
}

ModuleContext::PhantomScope ModuleContext::phantomScope(const PhantomsList& vars) {
  return {*this, vars};
}

bool ModuleContext::hasPhantom(const std::string& name) {
  for (auto scope = phantomScopes.rbegin(); scope != phantomScopes.rend(); ++scope) {
    if (scope->contains(name)) {
      return true;
    }
  }

  return false;
}

std::shared_ptr<meta::Type> ModuleContext::getPhantomType(const std::string& name) {
  for (auto scope = phantomScopes.rbegin(); scope != phantomScopes.rend(); ++scope) {
    if (scope->contains(name)) {
      return scope->at(name);
    }
  }

  return nullptr;
}

bool ModuleContext::hasLocal(const std::string& name) {
  for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
    if (scope->locals.has(name)) {
      return true;
    }
  }

  return false;
}

llvm::AllocaInst * ModuleContext::getLocalValue(const std::string& name) {
  for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
    if (scope->locals.has(name)) {
      return scope->locals[name]->value;
    }
  }

  return nullptr;
}

std::shared_ptr<meta::Type> ModuleContext::getLocalType(const std::string& name) {
  for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
    if (scope->locals.has(name)) {
      return scope->locals[name]->type;
    }
  }

  return nullptr;
}

void ModuleContext::addLocal(const std::string& name, std::shared_ptr<meta::TypedValue> tv) {
  assertThrow(!scopes.empty(), std::runtime_error("No scopes declared"));

  auto lifetime_start = llvm::Intrinsic::getOrInsertDeclaration(llvm.module.get(), llvm::Intrinsic::lifetime_start, {ir_builder->getPtrTy()});

  auto dl   = llvm.module->getDataLayout();
  auto size = dl.getTypeAllocSize(tv->type->getLLVMType(*this));

  ir_builder->CreateCall(
    lifetime_start,
    { ir_builder->getInt64(size), tv->value }
  );

  scopes.back().locals[name] = std::move(tv);
}

void ModuleContext::forgetLocal(const std::string& name) {
  scopes.back().forgotten.insert(name);
}

void ModuleContext::pushScope(SourceSpan span, llvm::DIScope * scope) {
  auto start = span.start();

  llvm::DIScope * parent = nullptr;

  if (!scope) {
    parent = scopes.empty() ? di_compile_unit : scopes.back().di_scope;
    scope = di_builder->createLexicalBlock(
      parent,
      di_compile_unit->getFile(),
      start.line,
      start.column
    );
  }

  ir_builder->SetCurrentDebugLocation(
    llvm::DILocation::get(*llvm.ctx, start.line, start.column, scope)
  );

  scopes.emplace_back(span, scope, globalContext.current_function);
}

void ModuleContext::popScope(bool no_clear) {
  if (!scopes.empty()) {
    auto end = scopes.back().span.end();

    ir_builder->SetCurrentDebugLocation(end.getDILocation(*this));
  }

  if (!no_clear) {
    scopes.back().clear(*this);
  }

  scopes.pop_back();
}

void ModuleContext::clearScopes(bool force) {
  for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
    if (scope->owner != globalContext.current_function) {
      break;
    }
    scope->clear(*this, force);
  }
}

Scope& ModuleContext::currentScope() {
  assertThrow(!scopes.empty(), std::runtime_error("No scopes declared"));

  return scopes.back();
}


llvm::DIScope * ModuleContext::currentDIScope() {
  if (scopes.empty()) {
    return di_compile_unit;
  }

  return scopes.back().di_scope;
}

void ModuleContext::setDebugLocation(SourceSpan span, llvm::DIScope * scope) {
  if (!scope) {
    scope = currentDIScope();
  }

  auto start = span.start();

  ir_builder->SetCurrentDebugLocation(llvm::DILocation::get(scope->getContext(), start.line, start.column, scope));
}

llvm::AllocaInst * ModuleContext::createEntryBlockAlloca(llvm::Type * type, const std::string& name) const {
  // Find the entry block of the current function
  llvm::Function *  fn         = ir_builder->GetInsertBlock()->getParent();
  llvm::BasicBlock& entryBlock = fn->getEntryBlock();

  // Create a temporary builder that points to the beginning of the entry block
  // If there are already allocas there, it's best to put it at the start
  llvm::IRBuilder<> tmpBuilder(&entryBlock, entryBlock.begin());

  return tmpBuilder.CreateAlloca(type, nullptr, name);
}

llvm::Value * ModuleContext::createFatPointerFromGlobalFunction(llvm::Function * fn, llvm::Type * fat_ptr_type) {
  std::string trampoline_name = fn->getName().str() + "$trampoline";

  llvm::Function * trampoline = llvm.module->getFunction(trampoline_name);

  if (!trampoline) {
    std::vector<llvm::Type *> arg_types;
    arg_types.push_back(ir_builder->getPtrTy()); // Closure

    for (auto& arg : fn->args()) {
      arg_types.push_back(arg.getType());
    }

    auto * trampoline_signature = llvm::FunctionType::get(fn->getReturnType(), arg_types, fn->isVarArg());

    trampoline = llvm::Function::Create(
      trampoline_signature, llvm::Function::InternalLinkage, trampoline_name, llvm.module.get()
    );

    auto * current_block = ir_builder->GetInsertBlock();

    auto * entry = llvm::BasicBlock::Create(*llvm.ctx, "entry", trampoline);
    ir_builder->SetInsertPoint(entry);

    // Arguments of original function. Since it is not a lambda - it doesn't need the closure
    std::vector<llvm::Value *> forward_args;
    for (size_t i = 1; i < trampoline->arg_size(); ++i) {
      forward_args.push_back(trampoline->getArg(i));
    }

    auto * res = ir_builder->CreateCall(fn->getFunctionType(), fn, forward_args);

    if (fn->getReturnType()->isVoidTy()) {
      ir_builder->CreateRetVoid();
    } else {
      ir_builder->CreateRet(res);
    }

    // Restore block
    if (current_block) {
      ir_builder->SetInsertPoint(current_block);
    }
  }

  // Assemble fat pointer
  llvm::Value * fat_ptr = llvm::PoisonValue::get(fat_ptr_type);
  llvm::Value * closure = llvm::ConstantPointerNull::get(ir_builder->getPtrTy());

  fat_ptr = ir_builder->CreateInsertValue(fat_ptr, trampoline, 0, "fn_ptr");
  fat_ptr = ir_builder->CreateInsertValue(fat_ptr, closure,    1, "empty_closure_ptr");

  return fat_ptr;
}

void ModuleContext::addDIParameter(
  llvm::DISubprogram *         di_fn,
  const std::string&           name,
  const std::shared_ptr<meta::Type>& type,
  const SourceSpan&            span,
  size_t                       index,
  llvm::Value *                storage
) {
  llvm::DILocalVariable * di_param = di_builder->createParameterVariable(
    di_fn,
    name,
    index,
    getCurrentDIFile(),
    span.start().line,
    type->getDIType(*this),
    true
  );

  di_builder->insertDeclare(
    storage ? storage : getLocalValue(name),
    di_param,
    di_builder->createExpression(),
    span.start().getDILocation(*this, di_fn),
    ir_builder->GetInsertBlock()
  );
}

llvm::Value * codegen::cast(ModuleContext& ctx, llvm::Value * val, llvm::Type * target_type, SourceSpan span) {
  if (!val || !target_type) {
    throw std::runtime_error("codegen::cast received nullptr");
  }

  if (val->getType() == target_type) {
    return val;
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

  if (auto * fn = llvm::dyn_cast<llvm::Function>(val)) {
    // If the target type is a struct with exactly 2 elements, it's a fat pointer
    if (target_type->isStructTy() && target_type->getStructNumElements() == 2) {
      return ctx.createFatPointerFromGlobalFunction(fn, target_type);
    }
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
    }

    return ctx.ir_builder->CreateZExtOrBitCast(val, target_type);
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

  util::RawStreamCollector rsc_v;
  val->getType()->print(*rsc_v.stream());

  util::RawStreamCollector rsc_t;
  target_type->print(*rsc_t.stream());

  Error(ERROR_INVALID_CAST, span, "Tried to cast '{}' into '{}'", rsc_v.string(), rsc_t.string()).raise();
}

#pragma once

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/StandardInstrumentations.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Scalar/Reassociate.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>

#include <map>

#include "xcc/jit.h"
#include "xcc/ast.h"
#include "xcc/meta/value.h"
#include "xcc/meta/function.h"
#include "xcc/ast/fndecl.h"
#include "xcc/util/llvm.h"

namespace xcc::codegen {

constexpr char DEFAULT_MODULE_NAME[] = "<module>";

class ModuleContext;

/**
 * Global compiler context, holds functions/globals, global ModuleContext and JIT
 */
class GlobalContext {
public:
  /* Target Info (with all LLVM contexts) */
  util::Target target;

  /* JIT Context */
  std::unique_ptr<JIT> jit;

  /* Single context for all modules */
  llvm::orc::ThreadSafeContext tsc;

  /* Global Module */
  std::shared_ptr<ModuleContext> globalModule;

  /* Already processed modules, which are waiting to be flushed to JIT, or merged to an object file */
  std::vector<std::unique_ptr<ModuleContext>> pendingModules;

  /* Global Variable Types */
  std::unordered_map<std::string, std::shared_ptr<meta::Type>> globals;

  /* Functions */
  std::unordered_map<std::string, std::shared_ptr<meta::Function>> functions;

  /* Current Function Name */
  std::string current_function;

  /* Stack of module in order of processing */
  std::vector<std::string> moduleStack;

  /* Defined macros */
  std::unordered_map<std::string, std::shared_ptr<ast::Macro>> macros;

  /* Aliases for imported functions that are brought into scope */
  std::unordered_map<std::string, std::string> aliases;

public:
  GlobalContext(util::Target target);
  ~GlobalContext() = default;

  /**
   * Creates GlobalContext. Should be used instead of raw constructor
   */
  static std::unique_ptr<GlobalContext> create(util::Target target);

  /**
   * Creates new module (e.g. for a new function) tied to global context
   *
   * @param name Module name
   */
  std::unique_ptr<ModuleContext> createModule(const std::string& name = DEFAULT_MODULE_NAME);

  /**
   * Adds module to global context. Should be called, when module is ready
   */
  void addModule(std::unique_ptr<ModuleContext>& module);

  /**
   * Flushes all added modules to JIT
   */
  void flushModulesToJIT();

  /**
   * Merges all added modules into global module from global context
   */
  void mergeModules() const;

  void addFunction(const std::string& name, std::shared_ptr<meta::Function> fn);
  std::shared_ptr<meta::Function> getMetaFunction(const std::string& name);

  void setCurrentFunction(const std::string& name);
  void clearCurrentFunction();
  std::shared_ptr<meta::Function> getCurrentFunction();

  bool hasGlobal(const std::string& name);
  llvm::GlobalVariable * getGlobal(ModuleContext& ctx, const std::string& name);
  std::shared_ptr<meta::Type> getGlobalType(const std::string& name);

  void pushModule(const std::string& name);
  void popModule();
  [[nodiscard]] std::string getModulePrefix() const;
  [[nodiscard]] std::string getParentModulePrefix() const;

  void registerMacro(const std::string& name, std::shared_ptr<ast::Macro> macro);
  std::shared_ptr<ast::Macro> getMacro(const std::string& name) const;

  void addAlias(const std::string& name, const std::string& value, SourceSpan span);
  std::string aliased(const std::string& name);

  void runExpr(std::shared_ptr<ast::Node> expr);

  void runFunction(const std::string& name);
};

/**
 * Context for an LLVM module (basically a new one is created for every function)
 */
class ModuleContext {
public:
  using ScopedPhantomList = std::unordered_map<std::string, std::shared_ptr<meta::Type>>;

private:
  class ScopedPhantomVariables {
    ModuleContext&           module;
    std::vector<std::string> names;

  public:
    ScopedPhantomVariables(ModuleContext& module, const ScopedPhantomList& vars);
    ~ScopedPhantomVariables();
  };

  struct Scope {
    std::unordered_map<std::string, std::shared_ptr<meta::TypedValue>> locals;
    bool cleared = false;

    void clear(ModuleContext& ctx);
  };

public:
  /* Module name */
  std::string name;

  /* Global Context Handle */
  GlobalContext& globalContext;

  /* Top-Level LLVM Contexts */
  struct {
    llvm::LLVMContext *           ctx; // Must be set to globalContext.tsc.getContext()
    std::unique_ptr<llvm::Module> module;
  } llvm;

  /* LLVM IR Builder */
  std::unique_ptr<llvm::IRBuilder<>> ir_builder;

  /* Named values (variables/args) organized in scopes */
  std::vector<Scope> scopes;

  /* Phantom named values (variables/args), that need to be in AST generator's scope without being generated */
  std::map<std::string, std::shared_ptr<meta::Type>> phantomLocals;

#if USE_OPTIMIZATION
  /* Optimization Contexts */
  struct {
    std::unique_ptr<llvm::FunctionPassManager> fpm;
    std::unique_ptr<llvm::LoopAnalysisManager> lam;
    std::unique_ptr<llvm::FunctionAnalysisManager> fam;
    std::unique_ptr<llvm::CGSCCAnalysisManager> cgam;
    std::unique_ptr<llvm::ModuleAnalysisManager> mam;
    std::unique_ptr<llvm::PassInstrumentationCallbacks> pic;
    std::unique_ptr<llvm::StandardInstrumentations> si;
  } opt;
#endif

public:
  explicit ModuleContext(GlobalContext& global, const std::string& name = DEFAULT_MODULE_NAME, util::Target * target = nullptr);

  static std::unique_ptr<ModuleContext> create(GlobalContext& global, const std::string& name = DEFAULT_MODULE_NAME, util::Target * target = nullptr);

  llvm::Function * getFunction(const std::string& name);

  ScopedPhantomVariables phantomScope(const ScopedPhantomList& vars);
  bool hasPhantom(const std::string& name);
  std::shared_ptr<meta::Type> getPhantomType(const std::string& name);

  bool hasLocal(const std::string& name);
  llvm::AllocaInst * getLocalValue(const std::string& name);
  std::shared_ptr<meta::Type> getLocalType(const std::string& name);
  void addLocal(const std::string& name, std::shared_ptr<meta::TypedValue> tv);

  void pushScope();
  void popScope();
  void clearScopes();
};

/**
 * Generate a cast
 *
 * @param ctx ModuleContext
 * @param val Value to be cas
 * @param target_type LLVM Type for `val` to be cast into
 * @param span Site of cast
 * @return Value generated by selected cast instruction
 */
llvm::Value * cast(ModuleContext& ctx, llvm::Value * val, llvm::Type * target_type, SourceSpan span);

/**
 * Calls cast() if types are not the same
 *
 * @param ctx ModuleContext
 * @param val Value to be cas
 * @param target_type LLVM Type for `val` to be cast into
 * @param span Site of cast
 * @return If val type and target_type differ - result of cast(), otherwise - val
 */
inline llvm::Value * castIfNotSame(ModuleContext& ctx, llvm::Value * val, llvm::Type * target_type, SourceSpan span) {
  if (!util::compareTypes(val->getType(), target_type)) {
    return codegen::cast(ctx, val, target_type, span);
  }
  return val;
}

/**
 * Register built-in macros into GlobalContext
 */
void registerBuiltinMacros(GlobalContext& ctx);

}


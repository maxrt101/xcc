#pragma once

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include "llvm/IR/DIBuilder.h"
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
#include "xcc/parser.h"
#include "xcc/meta/value.h"
#include "xcc/meta/function.h"
#include "xcc/ast/fndecl.h"
#include "xcc/util/llvm.h"

namespace xcc::codegen {

constexpr char DEFAULT_MODULE_NAME[] = "<module>";

class ModuleContext;

/**
 * Global generics cache
 *
 * All generic declarations aren't placed into the AST, instead
 * they are registered into this cache, and retrieved upon instantiation
 */
class GenericsCache {
public:
  /**
   * Return @c true if the cache contains a generic declaration with `name`
   */
  static bool has(const std::string& name);

  /**
   * Return generic declaration (if it exists) by `name`
   */
  static std::shared_ptr<ast::Node> get(const std::string& name);

  /**
   * Add a generic declaration into the cache
   */
  static void add(const std::string& name, std::shared_ptr<ast::Node> generic);

private:
  static std::unordered_map<std::string, std::shared_ptr<ast::Node>> cache;
};

/**
 * Global compiler context, holds functions/globals, global ModuleContext and JIT
 */
class GlobalContext {
public:
  struct Function {
    std::shared_ptr<meta::Function> meta_fn;
    std::shared_ptr<meta::Type>     meta_type;
  };

public:
  /* Target Info (with all LLVM contexts) */
  util::Target target;

  /* JIT Context */
  std::unique_ptr<JIT> jit;

  /* Single context for all modules */
  llvm::orc::ThreadSafeContext tsc;

  /* Global Module */
  std::shared_ptr<ModuleContext> globalModule;

  /* LLVM DebugInfo Builder */
  std::unique_ptr<llvm::DIBuilder> di_builder;

  /* LLVM DebugInfo Compile Unit */
  llvm::DICompileUnit * di_compile_unit = nullptr;

  /* Top-level DebugInfo File */
  llvm::DIFile * di_file = nullptr;

  /* Already processed modules, which are waiting to be flushed to JIT, or merged to an object file */
  std::vector<std::unique_ptr<ModuleContext>> pendingModules;

  /* Global Variable Types */
  std::unordered_map<std::string, std::shared_ptr<meta::Type>> globals;

  /* Functions */
  std::unordered_map<std::string, Function> functions;

  /* Current Function Name */
  std::string current_function;

  /* Stack of module in order of processing */
  std::vector<std::string> moduleStack;

  /* Defined macros */
  std::unordered_map<std::string, std::shared_ptr<ast::Macro>> macros;

  /* Aliases for imported functions that are brought into scope */
  std::unordered_map<std::string, std::string> aliases;

  /* Constants */
  std::unordered_map<std::string, std::shared_ptr<ast::ConstDecl>> consts;

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

  /**
   * Creates llvm::DICompileUnit for currently processed file
   */
  void createCompileUnit(FileId fileId);

  /**
   * Returns a reference to IncludedModule of currently processed Module
   *
   * @warning Will throw an exception if currently in top-level scope
   * @warning Will throw an exception if no module with such nam is present in ModuleCache
   */
  IncludedModule& getCurrentModule();

  /**
   * Return current llvm::DIFIle, returns a file for currently processed module,
   * if in module context, and file of CU if in top-level scope
   */
  llvm::DIFile * getCurrentDIFile();

  /**
   * Add a (meta) function record to internal function table
   */
  void addFunction(const std::string& name, std::shared_ptr<meta::Function> fn, std::shared_ptr<meta::Type> type);

  /**
   * Retrieve function record from function table by name
   * Returns nullptr if no such function is present
   */
  std::shared_ptr<meta::Function> getMetaFunction(const std::string& name);

  /**
   * Retrieve function record type from function table by name
   * Returns nullptr if no such function is present
   */
  std::shared_ptr<meta::Type> getMetaFunctionType(const std::string& name);

  /** Set name for currently processed function */
  void setCurrentFunction(const std::string& name);

  /** Clear current function */
  void clearCurrentFunction();

  /** Return a function record for currently processed function. Relies on setCurrentFunction */
  std::shared_ptr<meta::Function> getCurrentFunction();

  /** Return true if a global variable with provided name exists */
  bool hasGlobal(const std::string& name);

  /** Retrieve global variable by name */
  llvm::GlobalVariable * getGlobal(ModuleContext& ctx, const std::string& name);

  /** Get type of global variable by name */
  std::shared_ptr<meta::Type> getGlobalType(const std::string& name);

  /** Push a module into currently processed module stack */
  void pushModule(const std::string& name);

  /** Pop a module from currently processed module stack */
  void popModule();

  /** Get module prefix (a::b -> a_b_). Used by module symbol mangling. Relies on pushModule */
  [[nodiscard]] std::string getModulePrefix() const;

  /** Get parent module prefix (a::b -> a_). Used by module symbol mangling. Relies on pushModule */
  [[nodiscard]] std::string getParentModulePrefix() const;

  /** Register macro in internal macro map */
  void registerMacro(const std::string& name, std::shared_ptr<ast::Macro> macro);

  /** Retrieve registered macro from macro map */
  [[nodiscard]] std::shared_ptr<ast::Macro> getMacro(const std::string& name) const;

  /**
   * Add identifier alias
   * Used in scoped `use` statements:
   * `use a::{b, c}`: module a in included fully with a::b & a::c aliased into current scope as b & c
   */
  void addAlias(const std::string& name, const std::string& value, SourceSpan span);

  /** Recursively search for `name` in alias map, returning 'true' name, or `name` if not found */
  std::string aliased(const std::string& name);

  /** Add constant declaration to global const pool */
  void addConst(const std::string& name, std::shared_ptr<ast::ConstDecl> constant);

  /** Get constant from global const pool */
  std::shared_ptr<ast::ConstDecl> getConst(const std::string& name) const;

  /**
   * Run provided expression using JIT
   *
   * @note Expression will be put into anonymous function, which will be run using runFunction
   */
  void runExpr(std::shared_ptr<ast::Node> expr);

  /**
   * Run function using JIT
   *
   * @note Function needs to be compiled, and flushModulesToJIT called
   */
  void runFunction(const std::string& name);
};

/**
 * Context for an LLVM module (basically a new one is created for every function)
 */
class ModuleContext {
public:
  /** Map of scoped phantom variables, used as an initializer to PhantomScope */
  using PhantomsList = std::unordered_map<std::string, std::shared_ptr<meta::Type>>;

  /**
   * Holds 'phantom' variables
   *
   * Phantom variables are used, when a type for expression is needed, but can't be
   * generated out-of-line because it is dependent on sequential evaluation.
   * For example, let's consider this:
   * ```
   * var b = { var a: i32 = 30; a += 12; a }
   * ```
   *
   * When generating IR for this statement, the compiler needs to know the type for `b`,
   * because type annotation was provided - type can be retrieved from initializer value.
   * Because initializer value can be a block - compiler will call getOrGetLast, which
   * will return the node, or the last node in block if the node is a block.
   * When trying to generate a type for last value in this block, compiler will fail,
   * because the block wasn't evaluated so ast::Identifier("a") isn't registered anywhere
   * and can't provide a type in on itself. So here phantom variables come in handy -
   * a `phantomScope()` is called on a module to create a RAII PhantomScope
   * instance, to which all variable declarations are tied. So then `generateType()` is
   * called on initializer value of `b`, the compiler will try to find `a` in `phantomLocals`
   * map, and successfully generate a type for `b`.
   */
  class PhantomScope {
    ModuleContext&                                               module;

  public:
    PhantomScope(ModuleContext& module, const PhantomsList& vars);
    ~PhantomScope();

    void add(const std::string& name, std::shared_ptr<meta::Type> type);
  };

  /**
   * Represents a lexical scope in the codegen
   *
   * Holds span for block ('{}'), DebugInfo scope, locals map & 'cleared' flag
   * Cleared flag is used to signal that this scope was already finished, and it
   * just waits to be popped from scope stack.
   * Clearing means that local variables are marked as `freed` to the LLVM, this
   * enables some stack-space optimization later on
   */
  struct Scope {
    SourceSpan                                                         span;
    llvm::DIScope *                                                    di_scope;
    std::unordered_map<std::string, std::shared_ptr<meta::TypedValue>> locals;
    bool                                                               cleared = false;

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
  std::vector<std::unordered_map<std::string, std::shared_ptr<meta::Type>>> phantomScopes;

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

  /** Try to get function from current module, if fails - try to get from global module */
  llvm::Function * getFunction(const std::string& name);

  /** Creates a RAII Ph[antomScope, provided with a list of variables and their types */
  PhantomScope phantomScope(const PhantomsList& vars);

  /** Returns true if phantom variable with provided name exists */
  bool hasPhantom(const std::string& name);

  /** Retrieves type of specific phantom variable */
  std::shared_ptr<meta::Type> getPhantomType(const std::string& name);

  /** Returns true if local variable with provided name exists. Searches recursively up the scope stack */
  bool hasLocal(const std::string& name);

  /** Retrieves value of specific local variable */
  llvm::AllocaInst * getLocalValue(const std::string& name);

  /** Retrieves type of specific local variable */
  std::shared_ptr<meta::Type> getLocalType(const std::string& name);

  /** Save local variable record in current scope */
  void addLocal(const std::string& name, std::shared_ptr<meta::TypedValue> tv);

  /** Create new lexical scope and push it to scope stack. `scope` can be used to override default DIScope creation */
  void pushScope(SourceSpan span, llvm::DIScope * scope = nullptr);

  /** Pop lexical scope from scope stack */
  void popScope();

  /** Clear all scopes. Very dangerous, can break everything if called in the wrong place */
  void clearScopes();

  /**
   * Returns a reference to current Scope
   * @warning Will throw an exception, if no scopes are present
   */
  Scope& currentScope();

  /** Returns current DIScope. If scope stack is not empty - return current Scope's di_scope, otherwise returns CU scope */
  llvm::DIScope * currentDIScope();

  /** Set current IR debug location from SourceSpan */
  void setDebugLocation(SourceSpan span, llvm::DIScope * scope = nullptr);

  /** Creates an AllocaInst at the start of current InsertBlock */
  llvm::AllocaInst * createEntryBlockAlloca(llvm::Type * type, const std::string& name) const;

  /** Create a fat pointer ({callee, closure}) from a global function */
  llvm::Value * createFatPointerFromGlobalFunction(llvm::Function * fn, llvm::Type * fat_ptr_type);

  /** Create and add DIParameter to DebugInfo */
  void addDIParameter(
    llvm::DISubprogram *               di_fn,
    const std::string&                 name,
    const std::shared_ptr<meta::Type>& type,
    const SourceSpan&                  span,
    size_t                             index,
    llvm::Value *                      storage = nullptr
  );
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


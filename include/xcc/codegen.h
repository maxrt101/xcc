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
#include "xcc/meta/value.h"
#include "xcc/meta/function.h"
#include "xcc/ast/fndecl.h"
#include "xcc/util/llvm.h"

namespace xcc::codegen {

constexpr char DEFAULT_MODULE_NAME[] = "<module>";

class ModuleContext;

class GlobalContext {
public:
  /* JIT Context */
  std::unique_ptr<JIT> jit;

  /* Functions */
  std::map<std::string, std::shared_ptr<meta::Function>> functions;

  /*  */
  std::string current_function;

public:
  GlobalContext();
  ~GlobalContext() = default;

  static std::unique_ptr<GlobalContext> create();

  std::unique_ptr<ModuleContext> createModule(const std::string& name = DEFAULT_MODULE_NAME);

  void addModule(std::unique_ptr<ModuleContext>& module);

  void addFunction(const std::string& name, std::shared_ptr<meta::Function> fn);
  std::shared_ptr<meta::Function> getMetaFunction(const std::string& name);

  void setCurrentFunction(const std::string& name);
  void clearCurrentFunction();
  std::shared_ptr<meta::Function> getCurrentFunction();

  void runExpr(std::shared_ptr<ast::Node> expr);
};

class ModuleContext {
public:
  /* Global Context Handle */
  GlobalContext& globalContext;

  /* Top-Level LLVM Contexts */
  struct {
    std::unique_ptr<llvm::LLVMContext> ctx;
    std::unique_ptr<llvm::Module> module;
  } llvm;

  /* LLVM IR Builder */
  std::unique_ptr<llvm::IRBuilder<>> ir_builder;

  /* Named values (variables/args) */
  std::map<std::string, std::shared_ptr<meta::TypedValue>> namedValues;

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

public:
  ModuleContext(GlobalContext& global, const std::string& name = DEFAULT_MODULE_NAME);

  static std::unique_ptr<ModuleContext> create(GlobalContext& global, const std::string& name = DEFAULT_MODULE_NAME);

  llvm::Function * getFunction(const std::string& name);
};

llvm::Value * cast(ModuleContext& ctx, llvm::Value * val, llvm::Type * target_type);

inline llvm::Value * castIfNotSame(ModuleContext& ctx, llvm::Value * val, llvm::Type * target_type) {
  if (!util::compareTypes(val->getType(), target_type)) {
    return codegen::cast(ctx, val, target_type);
  }
  return val;
}

}


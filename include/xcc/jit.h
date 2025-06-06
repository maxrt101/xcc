#pragma once

#include <memory>

#include <llvm/ADT/StringRef.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/Shared/ExecutorSymbolDef.h>
#include <llvm/IR/DataLayout.h>

namespace xcc::codegen {

/**
 * Just In Time compilation context
 */
class JIT {
private:
  std::unique_ptr<llvm::orc::ExecutionSession> session;

  llvm::orc::MangleAndInterner mangle;
  llvm::DataLayout data_layout;

  llvm::orc::RTDyldObjectLinkingLayer object_layer;
  llvm::orc::IRCompileLayer compile_layer;

  llvm::orc::JITDylib& main_jd;

public:
  JIT(std::unique_ptr<llvm::orc::ExecutionSession> session, llvm::orc::JITTargetMachineBuilder jtmb, llvm::DataLayout layout);
  ~JIT();

  static std::unique_ptr<JIT> create();

  const llvm::DataLayout& getDataLayout() const;
  llvm::orc::JITDylib& getMainJitDylib();
  llvm::Error addModule(llvm::orc::ThreadSafeModule tsm, llvm::orc::ResourceTrackerSP rt = nullptr);
  llvm::Expected<llvm::orc::ExecutorSymbolDef> lookup(llvm::StringRef name);

  void dump();
};

}

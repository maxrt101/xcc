#include "xcc/jit.h"
#include "xcc/exceptions.h"

using namespace xcc::codegen;

class SymbolResolverGenerator : public llvm::orc::DefinitionGenerator {
public:
  llvm::orc::MangleAndInterner& mangle;

public:
  explicit SymbolResolverGenerator(llvm::orc::MangleAndInterner& mangle) : mangle(mangle) {}

  static std::unique_ptr<SymbolResolverGenerator> create(llvm::orc::MangleAndInterner& mangle) {
    return std::make_unique<SymbolResolverGenerator>(mangle);
  }

  llvm::Error tryToGenerate(llvm::orc::LookupState &state, llvm::orc::LookupKind kind, llvm::orc::JITDylib &jd,
                            llvm::orc::JITDylibLookupFlags flags, const llvm::orc::SymbolLookupSet& set) override {
    for (auto& [sym_name, sym_flags] : set) {
      auto name = llvm::orc::SymbolStringPoolEntryUnsafe::from(sym_name).rawPtr()->first().str();

      auto sym = jd.getExecutionSession().lookup({&jd}, mangle(name));

      if (sym) {
#if USE_REPORT_SYMBOL_RESOLVER_SUCCESS
        printf("Found symbol '%s' (mangled '%s')\n", name.c_str(),
               llvm::orc::SymbolStringPoolEntryUnsafe::from(mangle(name)).rawPtr()->first().str().c_str());
#endif

        llvm::orc::SymbolMap symbols;

        symbols[jd.getExecutionSession().getSymbolStringPool()->intern(name)] = sym.get();

        llvm::cantFail(jd.define(llvm::orc::absoluteSymbols(std::move(symbols))));

        continue;
      }

      return llvm::createStringError("Unable to resolve symbol %s", name.c_str());
    }

    return llvm::Error::success();
  }
};

JIT::JIT(std::unique_ptr<llvm::orc::ExecutionSession> session, llvm::orc::JITTargetMachineBuilder jtmb, llvm::DataLayout layout)
  : session(std::move(session)), data_layout(layout), mangle(*this->session, this->data_layout),
    object_layer(*this->session, []() { return std::make_unique<llvm::SectionMemoryManager>(); }),
    compile_layer(*this->session, this->object_layer, std::make_unique<llvm::orc::ConcurrentIRCompiler>(std::move(jtmb))),
    main_jd(this->session->createBareJITDylib("<main>")) {

  llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

  main_jd.addGenerator(llvm::cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(data_layout.getGlobalPrefix())));
  main_jd.addGenerator(SymbolResolverGenerator::create(mangle));

  if (jtmb.getTargetTriple().isOSBinFormatCOFF()) {
    object_layer.setOverrideObjectFlagsWithResponsibilityFlags(true);
    object_layer.setAutoClaimResponsibilityForObjectSymbols(true);
  }
}

JIT::~JIT() {
  if (auto err = session->endSession()) {
    session->reportError(std::move(err));
  }
}

std::unique_ptr<JIT> JIT::create() {
  auto epc = llvm::orc::SelfExecutorProcessControl::Create();

  if (!epc) {
    throw CodegenException(epc.takeError());
  }

  auto session = std::make_unique<llvm::orc::ExecutionSession>(std::move(*epc));

  llvm::orc::JITTargetMachineBuilder jtmb(session->getExecutorProcessControl().getTargetTriple());

  auto data_layout = jtmb.getDefaultDataLayoutForTarget();

  if (!data_layout) {
    throw CodegenException(data_layout.takeError());
  }

  return std::make_unique<JIT>(std::move(session), std::move(jtmb), std::move(*data_layout));
}

const llvm::DataLayout& JIT::getDataLayout() const {
  return data_layout;
}

llvm::orc::JITDylib& JIT::getMainJitDylib() {
  return main_jd;
}

llvm::Error JIT::addModule(llvm::orc::ThreadSafeModule tsm, llvm::orc::ResourceTrackerSP rt) {
  if (!rt) {
    rt = main_jd.getDefaultResourceTracker();
  }

  return compile_layer.add(rt, std::move(tsm));
}

llvm::Expected<llvm::orc::ExecutorSymbolDef> JIT::lookup(llvm::StringRef name) {
  return session->lookup({&main_jd}, name.str());
}

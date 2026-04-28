#include "xcc/xcc.h"
#include "xcc/attrs.h"
#include "xcc/util/fs.h"

#include <llvm/IR/LegacyPassManager.h>

using namespace xcc;

static auto logger = xcc::util::log::Logger("XCC");

/**
 * Process attributes for top-level block
 *
 * @param block AST Block
 */
static void processAttributes(const std::shared_ptr<ast::Block>& block) {
  for (auto& node : block->body) {
    if (!node->attributes.empty()) {
      for (auto& attr : node->attributes) {
        attr::callHandler(attr, node.get());
      }
    }
  }
}

/**
 * Compile function (both declarations & definitions)
 *
 * @param globalContext Global Context
 * @param node          FnDecl/FnDef node
 */
static llvm::Function * compileFunction(std::unique_ptr<codegen::GlobalContext>& globalContext, std::shared_ptr<ast::Node> node) {
  if (node->isAnyOf(ast::AST_FUNCTION_DEF, ast::AST_FUNCTION_DECL)) {
    auto decl = node->is(ast::AST_FUNCTION_DEF) ? node->as<ast::FnDef>()->decl.get() : node->as<ast::FnDecl>();
    auto name = decl->name->as<ast::Identifier>()->name();

    auto ctx = globalContext->createModule(name);

    auto fn = node->generateFunction(*ctx, {});

#if USE_PRINT_LLVM_IR
    logger.info("LLVM IR for function {}:", fn->getName().str());
    util::RawStreamCollector collector;
    fn->print(*collector.stream());
    logger.print("{}", collector.string());
#endif

    globalContext->addModule(ctx);
    return fn;
  }

  throw CodegenException(std::format("compileFunction expects AST_FUNCTION_DECL or AST_FUNCTION_DEF, got {}", ast::Node::typeToString(node->type)));
}

/**
 * Recursively lowers AST
 *
 * @param globalContext Global Context
 * @param result        Generates functions and/or top-level expressions will be put here
 * @param block         Block to lower
 * @param isRepl        In in REPL mode
 */
static void compileBlock(
  std::unique_ptr<codegen::GlobalContext>& globalContext,
  CompilationResult&                       result,
  const std::shared_ptr<ast::Block>&       block,
  bool                                     isRepl
) {
  for (auto& node : block->body) {
    if (node->isAnyOf(ast::AST_FUNCTION_DEF, ast::AST_FUNCTION_DECL)) {
      compileFunction(globalContext, node);
    } else if (node->is(ast::AST_VAR_DECL)) {
      node->generateValue(*globalContext->globalModule, {});
    } else if (node->is(ast::AST_STRUCT)) {
      node->generateType(*globalContext->globalModule, {});
      for (auto& method : node->as<ast::Struct>()->methods) {
        compileFunction(globalContext, method);
      }
    } else if (node->is(ast::AST_MOD)) {
      globalContext->pushModule(node->as<ast::Module>()->getName());
      compileBlock(globalContext, result, node->as<ast::Module>()->body, isRepl);
      globalContext->popModule();
    } else if (node->is(ast::AST_TYPE_DECL)) {
      node->generateType(*globalContext->globalModule, {});
    } else if (node->is(ast::AST_EMPTY)) {
      // Ignore
    } else {
      if (isRepl) {
        result.nodes.expr.push_back(node);
      } else {
        throw std::runtime_error("Unexpected node at top-level scope: " + ast::Node::typeToString(node->type));
      }
    }
  }
}

util::Target xcc::init(const std::string& target, const std::string& machine, bool autoCleanup) {
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  if (autoCleanup) {
    std::atexit(xcc::cleanup);
  }

  auto target_info = util::Target(target, machine);

  logger.info("XCC v{} initialized for {}", getVersion().c_str(), target_info.target_triple);

  setenv("XCC_VERSION", getVersion().c_str(), 1);

  return target_info;
}

void xcc::cleanup() {
  logger.debug("Cleaning up");
  util::log::cleanup();
}

CompilationResult xcc::compile(
  std::unique_ptr<codegen::GlobalContext>& globalContext,
  const std::string&                       src,
  bool                                     isRepl,
  const std::vector<std::string>&          includePaths
) {
  auto lexer  = Lexer(src);
  auto tokens = lexer.tokenize();

#if USE_PRINT_TOKENS
  logger.info("TOKENS:");
  for (auto& token : tokens) {
    std::string value = token.value;
    if (token.is(TokenType::TOKEN_STRING)) {
      value = util::strescseq(value, false);
    }
    logger.print("{:<20} '{}'\n", Token::typeToString(token.type), value);
  }
#endif

  auto parser = Parser(tokens);

  for (auto& path : includePaths) {
    parser.addModuleSearchPath(path);
  }

  auto ast = parser.parse(isRepl);

  processAttributes(ast);

#if USE_PRINT_AST
  logger.info("AST:");
  ast::printAst(ast);
#endif

  CompilationResult result;

  compileBlock(globalContext, result, ast, isRepl);

  return result;
}

void xcc::compile_to_object(
  std::unique_ptr<codegen::GlobalContext>& globalContext,
  const std::string&                       src,
  const std::string&                       filename,
  const std::vector<std::string>&          includePaths
) {
  std::error_code error;
  llvm::raw_fd_ostream dest(filename, error, llvm::sys::fs::OF_None);

  if (error) {
    logger.error("Could not open file {}: {}", filename, error.message());
    throw std::runtime_error("Failed to open output file");
  }

  llvm::legacy::PassManager pass;
  auto file_type = llvm::CodeGenFileType::ObjectFile;

  if (globalContext->target.machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
    logger.error("TargetMachine can't emit an object file");
    throw std::runtime_error("TargetMachine can't emit an object file");
  }

  compile(globalContext, src, false, includePaths);

  globalContext->mergeModules();

  pass.run(*globalContext->globalModule->llvm.module);
  dest.flush();
}

void xcc::run(
  std::unique_ptr<codegen::GlobalContext>& globalContext,
  const std::string&                       src,
  bool                                     isRepl,
  const std::vector<std::string>&          includePaths
) {
  auto result = compile(globalContext, src, isRepl, includePaths);

  globalContext->flushModulesToJIT();

  if (isRepl) {
    if (!result.nodes.expr.empty()) {
      globalContext->runExpr(ast::Block::create(result.nodes.expr));
    }
  } else {
    globalContext->runFunction("main");
  }
}

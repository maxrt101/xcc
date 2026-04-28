#include "xcc/xcc.h"
#include "xcc/attrs.h"
#include "xcc/util/fs.h"
#include "xcc/util/string.h"

#include <llvm/IR/LegacyPassManager.h>

static auto logger = xcc::util::log::Logger("XCC");

/**
 * Process attributes for top-level block
 *
 * @param block AST Block
 */
static void process_attributes(const std::shared_ptr<xcc::ast::Block>& block) {
  for (auto& node : block->body) {
    if (!node->attributes.empty()) {
      for (auto& attr : node->attributes) {
        xcc::attr::callHandler(attr, node.get());
      }
    }
  }
}

/**
 * Recursively lowers AST
 *
 * @param globalContext Global Context
 * @param result        Generates functions and/or top-level expressions will be put here
 * @param block         Block to lower
 * @param isRepl        In in REPL mode
 */
static void process_ast_block(
  std::unique_ptr<xcc::codegen::GlobalContext>& globalContext,
  xcc::CompilationResult&                       result,
  const std::shared_ptr<xcc::ast::Block>&       block,
  bool                                          isRepl
) {
  for (auto& node : block->body) {
    if (node->isAnyOf(xcc::ast::AST_FUNCTION_DEF, xcc::ast::AST_FUNCTION_DECL)) {
      result.nodes.fn.push_back(node);
    } else if (node->is(xcc::ast::AST_VAR_DECL)) {
      node->generateValue(*globalContext->globalModule, {});
    } else if (node->is(xcc::ast::AST_STRUCT)) {
      node->generateType(*globalContext->globalModule, {});
      for (auto& method : node->as<xcc::ast::Struct>()->methods) {
        result.nodes.fn.push_back(method);
      }
    } else if (node->is(xcc::ast::AST_MOD)) {
      process_ast_block(globalContext, result, node->as<xcc::ast::Module>()->body, isRepl);
    } else if (node->is(xcc::ast::AST_EMPTY)) {
      // Ignore
    } else {
      if (isRepl) {
        result.nodes.expr.push_back(node);
      } else {
        throw std::runtime_error("Unexpected node at top-level scope: " + xcc::ast::Node::typeToString(node->type));
      }
    }
  }
}

xcc::util::Target xcc::init(const std::string& target, const std::string& machine, bool autoCleanup) {
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

  return target_info;
}

void xcc::cleanup() {
  logger.debug("Cleaning up");
  util::log::cleanup();
}

xcc::CompilationResult xcc::compile(
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

  process_attributes(ast);

#if USE_PRINT_AST
  logger.info("AST:");
  ast::printAst(ast);
#endif

  CompilationResult result;

  process_ast_block(globalContext, result, ast, isRepl);

  for (auto& node : result.nodes.fn) {
    if (node->isAnyOf(ast::AST_FUNCTION_DEF, ast::AST_FUNCTION_DECL)) {
      auto decl = node->is(ast::AST_FUNCTION_DEF) ? node->as<ast::FnDef>()->decl.get() : node->as<ast::FnDecl>();
      auto name = decl->name->as<ast::Identifier>()->name();

      auto ctx = globalContext->createModule(name);

#if USE_PRINT_LLVM_IR
      auto fn = node->generateFunction(*ctx, {});
      logger.info("LLVM IR for function {}:", fn->getName().str());
      util::RawStreamCollector collector;
      fn->print(*collector.stream());
      logger.print("{}", collector.string());
#else
      node->generateFunction(*ctx, {});
#endif
      globalContext->addModule(ctx);
    } else {
      throw std::runtime_error("Unexpected node at top-level scope: " + ast::Node::typeToString(node->type));
    }
  }

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

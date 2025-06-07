#include "xcc/xcc.h"
#include "xcc/util/string.h"

static auto logger = xcc::util::log::Logger("XCC");

void xcc::init(bool autoCleanup) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  if (autoCleanup) {
    std::atexit(xcc::cleanup);
  }

  logger.info("XCC v{} initialized", getVersion().c_str());
}

void xcc::cleanup() {
  logger.debug("Cleaning up");
  util::log::cleanup();
}

void xcc::run(std::unique_ptr<codegen::GlobalContext>& globalContext, const std::string& src, bool isRepl) {
  auto tokens = Lexer(src).tokenize();

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

  auto ast = Parser(tokens).parse(isRepl);

#if USE_PRINT_AST
  logger.info("AST:");
  ast::printAst(ast);
#endif

  std::vector<std::shared_ptr<ast::Node>> fn_nodes;
  std::vector<std::shared_ptr<ast::Node>> expr_nodes;

  for (auto& node : ast->body) {
    if (node->isAnyOf(ast::AST_FUNCTION_DEF, ast::AST_FUNCTION_DECL)) {
      fn_nodes.push_back(node);
    } else if (node->is(ast::AST_VAR_DECL)) {
      node->generateValue(*globalContext->globalModule, {});
    } else if (node->is(ast::AST_STRUCT)) {
      node->generateType(*globalContext->globalModule, {});
    } else {
      if (isRepl) {
        expr_nodes.push_back(node);
      } else {
        throw std::runtime_error("Unexpected node at top-level scope: " + ast::Node::typeToString(node->type));
      }
    }
  }

#if USE_PRINT_LLVM_IR
  logger.info("LLVM IR:");
  fflush(stdout);
#endif

  for (auto& node : fn_nodes) {
    auto ctx = globalContext->createModule();

    if (node->isAnyOf(ast::AST_FUNCTION_DEF, ast::AST_FUNCTION_DECL)) {
      auto fn = node->generateFunction(*ctx, {});
#if USE_PRINT_LLVM_IR
      fn->print(llvm::outs());
#endif
      globalContext->addModule(ctx);
    } else {
      throw std::runtime_error("Unexpected node at top-level scope: " + ast::Node::typeToString(node->type));
    }
  }

  if (isRepl) {
    if (!expr_nodes.empty()) {
      globalContext->runExpr(ast::Block::create(expr_nodes));
    }
  } else {
    globalContext->runFunction("main");
  }
}

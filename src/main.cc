#include <iostream>
#include <fstream>
#include <sstream>

#include "xcc/xcc.h"
#include "xcc/lexer.h"
#include "xcc/parser.h"
#include "xcc/codegen.h"
#include "xcc/util/string.h"

extern "C" [[maybe_unused]] int32_t xcc_putc(int32_t c) {
  fputc((char)c, stdout);
  return 0;
}

extern "C" [[maybe_unused]] int32_t xcc_putd(int32_t i) {
  printf("%d", i);
  return 0;
}

extern "C" [[maybe_unused]] int32_t xcc_putud(uint32_t i) {
  printf("%u", i);
  return 0;
}

extern "C" [[maybe_unused]] int32_t xcc_putux(uint32_t i) {
  printf("%x", i);
  return 0;
}

namespace xcc {

void run(std::unique_ptr<codegen::GlobalContext>& globalContext, const std::string& src, bool isRepl) {
  auto tokens = Lexer(src).tokenize();

#if USE_PRINT_TOKENS
  for (auto& token : tokens) {
    printf("%-20s '%s'\n", Token::typeToString(token.type).c_str(), token.value.c_str());
  }
#endif

  auto ast = Parser(tokens).parse(isRepl);

#if USE_PRINT_AST
  ast::printAst(ast);
#endif

  std::vector<std::shared_ptr<ast::Node>> fn_nodes;
  std::vector<std::shared_ptr<ast::Node>> expr_nodes;

  for (auto& node : ast->body) {
    if (node->isAnyOf(ast::AST_FUNCTION_DEF, ast::AST_FUNCTION_DECL)) {
      fn_nodes.push_back(node);
    } else {
      if (isRepl) {
        expr_nodes.push_back(node);
      } else {
        throw std::runtime_error("Unexpected node at top-level scope: " + ast::Node::typeToString(node->type));
      }
    }
  }

  for (auto& node : fn_nodes) {
    auto ctx = globalContext->createModule();

    if (node->isAnyOf(ast::AST_FUNCTION_DEF, ast::AST_FUNCTION_DECL)) {
      auto fn = node->generateFunction(*ctx);
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
    globalContext->runExpr(ast::Call::create(ast::Identifier::create("main"), {}));
  }
}

}

int main(int argc, char ** argv) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  auto globalContext = xcc::codegen::GlobalContext::create();

  if (argc == 2) {
    std::ifstream fs(argv[1]);
    std::stringstream ss;
    ss << fs.rdbuf();

    try {
      xcc::run(globalContext, ss.str(), false);
    } catch (std::exception& e) {
      printf("%s\n", e.what());
      return 1;
    }
    return 0;
  }

  printf("xcc (experimental) repl %s by maxrt\n", xcc::getVersion().c_str());

  while (true) {
    printf("-> ");

    std::string line;
    std::getline(std::cin, line);

    auto tokens = xcc::util::strsplit(line);

    if (tokens.empty()) {
      continue;
    }

    if (tokens[0] == "/quit" || tokens[0] == "/q") {
      break;
    }

    if (tokens[0] == "/help" || tokens[0] == "/h") {
      printf("/help or /h - Prints this message\n");
      printf("/quit or /q - Exits from REPL\n");
      printf("/list or /l - List global function symbols\n");
      continue;
    }

    if (tokens[0] == "/list") {
      for (auto& [name, fn] : globalContext->functions) {
        printf("%s\n", fn->toString().c_str());
      }
      continue;
    }

    // If user didn't put ';' at the end
    if (line.back() != ';') {
      line += ";";
    }

    try {
      // Run line from REPL
      xcc::run(globalContext, line, true);
    } catch (std::exception& e) {
      printf("%s\n", e.what());
    }
  }

  return 0;
}

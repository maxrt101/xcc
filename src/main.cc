#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "xcc/xcc.h"
#include "xcc/args.h"
#include "xcc/util/string.h"

#include <llvm/IR/LegacyPassManager.h>

static auto logger = xcc::util::log::Logger("MAIN");

#if USE_LEGACY_XCC_EXTERN_FUNCTIONS
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

extern "C" [[maybe_unused]] int32_t xcc_puts(int8_t * s) {
  printf("%s", s);
  return 0;
}
#endif

static std::string readFile(const std::string& filename) {
  std::ifstream fs(filename);

  if (!fs.is_open()) {
    logger.fatal("Failed to open file '{}'", filename);
    throw std::runtime_error("Failed to open the file");
  }

  std::stringstream ss;
  ss << fs.rdbuf();

  return ss.str();
}

static void help() {
  logger.println("XCC Compiler");
  logger.println("Usage: xcc [-h] [-v] [--verbose] [-c] [-r] [-t TARGET] [-m MACHINE] [-o OUT_FILE] IN_FILE...");
  logger.println("  -h, --help            - Print this message");
  logger.println("  -v, --version         - Print version");
  logger.println("  -c, --compile         - Compile into object file");
  logger.println("  -r, --run             - Run file using JIT");
  logger.println("  -t, --target TARGET   - Specify target triple");
  logger.println("  -m, --machine MACHINE - Specify target machine (cpu)");
  logger.println("  -o, --output OUT_FILE - Set output file name (for -c)");
  logger.println("  IN_FILE...            - Input (source) files");
}

static int compile(std::unique_ptr<xcc::codegen::GlobalContext> globalContext, xcc::args::Arguments& args) {
  if (!args.compile) {
    logger.error("Linking isn't supported for now");
    return 1;
  }

  std::error_code error;
  llvm::raw_fd_ostream dest(args.output, error, llvm::sys::fs::OF_None);

  if (error) {
    logger.error("Could not open file {}: {}", args.output, error.message());
    return 1;
  }

  llvm::legacy::PassManager pass;
  auto file_type = llvm::CodeGenFileType::ObjectFile;

  if (globalContext->target.machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
    logger.error("TargetMachine can't emit a file of this type");
    return 1;
  }

  xcc::compile(globalContext, readFile(args.files[0]), false);

  globalContext->mergeModules();

  globalContext->globalModule->llvm.module->print(llvm::errs(), nullptr);

  pass.run(*globalContext->globalModule->llvm.module);
  dest.flush();

  return 0;
}

static int run(std::unique_ptr<xcc::codegen::GlobalContext> globalContext, xcc::args::Arguments& args) {
#if USE_CATCH_EXCEPTIONS
  try {
#endif
    xcc::run(globalContext, readFile(args.files[0]), false);
#if USE_CATCH_EXCEPTIONS
  } catch (std::exception& e) {
    logger.fatal("{}", e.what());
    return 1;
  }
#endif

  return 0;
}

static int repl(std::unique_ptr<xcc::codegen::GlobalContext> globalContext, xcc::args::Arguments& args) {
  logger.print("xcc (experimental) repl {}\n", xcc::getVersion());

  while (true) {
    logger.print("xcc> ");

    std::string line;
    std::getline(std::cin, line);

    if (std::cin.eof()) {
      logger.print("EOF\n");
      break;
    }

    if (line.empty()) {
      continue;
    }

    auto tokens = xcc::util::strsplit(line);

    if (!tokens.empty() && tokens[0].starts_with("/")) {
      auto command = tokens[0].substr(1);

      if (command == "quit" || command == "q") {
        break;
      }

      if (command == "help" || command == "h") {
        logger.print("/help or /h - Prints this message\n");
        logger.print("/quit or /q - Exits from REPL\n");
        logger.print("/list or /l - List global function symbols\n");
        continue;
      }

      if (command == "list" || command == "l") {
        for (auto& [name, fn] : globalContext->functions) {
          logger.print("{}\n", fn->toString());
        }
        continue;
      }

      logger.error("Unknown command '{}'", command);
      continue;
    }

#if USE_CATCH_EXCEPTIONS
    try {
#endif
      xcc::run(globalContext, line, true);
#if USE_CATCH_EXCEPTIONS
    } catch (std::exception& e) {
      logger.error("{}\n", e.what());
    }
#endif
  }

  return 0;
}

int main(int argc, char ** argv) {
  auto args = xcc::args::parse(argc, argv);

  if (args.help) {
    help();
    return 0;
  }

  if (args.version) {
    logger.println("xcc {}", xcc::getVersion());
    return 0;
  }

  if ((args.compile && args.run) || (args.link && args.run)) {
    logger.error("--run cannot be used with --compile or --link");
    return 1;
  }

  auto target = xcc::init(args.target, args.machine);

  auto globalContext = xcc::codegen::GlobalContext::create();

  globalContext->setTarget(target);

  if (!args.files.empty()) {
    if (args.run) {
      return run(std::move(globalContext), args);
    }

    return compile(std::move(globalContext), args);
  }

  return repl(std::move(globalContext), args);
}

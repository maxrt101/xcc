#include <iostream>
#include <fstream>
#include <sstream>

#include "xcc/xcc.h"
#include "xcc/args.h"
#include "xcc/util/string.h"
#include "xcc/util/env.h"
#include "xcc/util/fs.h"
#include "xcc/util/filemng.h"

#include <llvm/TargetParser/Host.h>
#include <llvm/MC/MCSubtargetInfo.h>

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

static std::vector<std::string> getModPaths(const std::string& filepath, xcc::args::Arguments& args) {
  auto mod_paths = args.mod_paths;
  auto parent = xcc::fs::path::getParent(filepath);

  if (!parent.empty()) {
    mod_paths.insert(mod_paths.begin(), parent);
  }

  return mod_paths;
}

static int help() {
  fprintf(stderr, "XCC Compiler v%s\n", xcc::getVersion().c_str());
  fprintf(stderr, "Usage: xcc [-h] [-v] [--verbose] [-c] [-r] [-l LIB] [-L PATH] [-I PATH] [-t TARGET] [-m MACHINE] [-o OUT_FILE] IN_FILE...\n");
  fprintf(stderr, "Arguments:\n");
  fprintf(stderr, "  -h, --help              - Print this message\n");
  fprintf(stderr, "  -v, --version           - Print version\n");
  fprintf(stderr, "  -c, --compile           - Compile into object file\n");
  fprintf(stderr, "  -r, --run               - Run file using JIT\n");
  fprintf(stderr, "  -l, --lib LIB           - Link LIB\n");
  fprintf(stderr, "  -L, --lib-path LIB_PATH - Add library search path\n");
  fprintf(stderr, "  -I, --mod-path MOD_PATH - Add module search path\n");
  fprintf(stderr, "  -t, --target TARGET     - Specify target triple (use 'list' to see all)\n");
  fprintf(stderr, "  -m, --machine MACHINE   - Specify target machine (cpu) (use 'list' to see all)\n");
  fprintf(stderr, "  -o, --output OUT_FILE   - Set output file name\n");
  fprintf(stderr, "  --log LOG_MODULE_NAME   - Enable logger for module ('*' to enable for all)\n");
  fprintf(stderr, "  IN_FILE...              - Input (source/object) files\n");
  fprintf(stderr, "Environment:\n");
  fprintf(stderr, "  XCC_LD                  - Path to linker executable\n");
  fprintf(stderr, "  XCC_LDFLAGS             - Flags to pass directly to linker\n");
  return 0;
}

static int version() {
  logger.println("xcc {}", xcc::getVersion());
  return 0;
}

static int list_targets(xcc::args::Arguments& args) {
  xcc::init(llvm::sys::getDefaultTargetTriple(), args.machine);

  std::vector<std::tuple<std::string, std::string>> targets;
  size_t width = 0;

  for (const llvm::Target& t : llvm::TargetRegistry::targets()) {
    targets.emplace_back(t.getName(), t.getShortDescription());
    width = std::max(width, strlen(t.getName()));
  }

  for (auto & [name, desc] : targets) {
    logger.println("{:{}} - {}", name, width, desc);
  }

  return 0;
}

static int list_machines(xcc::args::Arguments& args) {
  auto target = xcc::init(args.target, "generic");

  std::unique_ptr<llvm::MCSubtargetInfo> sti(target.target->createMCSubtargetInfo(args.target, "", ""));

  if (!sti) {
    logger.error("Could not get subtarget info for triple {}", args.target);
    return 1;
  }

  for (auto & cpu : sti->getAllProcessorDescriptions()) {
    if (std::string(cpu.Key) != "generic") {
      logger.println("  {}", cpu.Key);
    }
  }

  return 0;
}

static int compile(std::unique_ptr<xcc::codegen::GlobalContext> globalContext, xcc::args::Arguments& args) {
  if (args.files.size() > 1) {
    logger.warn("Ignoring input files after '{}'", args.files[0]);
    logger.info("xcc in 'object-compile' mode accepts only one file");
  }

  auto filename = args.files[0];
  auto file = xcc::FileManager::load(filename);
  auto out = args.output.empty() ? xcc::fs::path::getFileName(filename) + ".o" : args.output;

  logger.info("Compiling '{}' into '{}'", filename, out);

  xcc::compile_to_object(globalContext, file, out, getModPaths(filename, args));

  return 0;
}

static int link(std::unique_ptr<xcc::codegen::GlobalContext> globalContext, xcc::args::Arguments& args) {
  std::vector<std::string> files_to_link;

  for (auto & filename : args.files) {
    auto file = xcc::FileManager::load(filename);

    auto magic = llvm::identify_magic(xcc::FileManager::get(file)->contents);

    if (magic.is_object()) {
      logger.info("Found object file '{}'", xcc::fs::path::getFileName(filename));
      files_to_link.push_back(filename);
    } else {
      auto out = xcc::fs::path::getFileName(filename) + ".o";

      logger.info("Compiling '{}' into '{}'", xcc::fs::path::getFileName(filename), xcc::fs::path::getFileName(out));

      xcc::compile_to_object(globalContext, file, out, getModPaths(filename, args));

      auto target = globalContext->target;
      globalContext = xcc::codegen::GlobalContext::create(target);
      files_to_link.push_back(out);
    }
  }

  auto ld = xcc::env::get("XCC_LD", "/usr/bin/ld");
  auto out = args.output.empty() ? "a.out" : args.output;
  auto cmd = std::format("{} -o {} ", ld, out);

  for (auto& filename : files_to_link) {
    cmd += filename + " ";
  }

  for (auto& path : args.lib_paths) {
    cmd += "-L" + path + " ";
  }

  for (auto& lib : args.libs) {
    cmd += "-l" + lib + " ";
  }

  cmd += xcc::env::get("XCC_LDFLAGS", "");

  logger.info("Linking {} objects into '{}'", files_to_link.size(), xcc::fs::path::getFileName(out));
  logger.debug("Linker command: {}", cmd);

  return std::system(cmd.c_str());
}

static int run(std::unique_ptr<xcc::codegen::GlobalContext> globalContext, xcc::args::Arguments& args) {
  for (auto& filename : args.files) {
    logger.info("Running file '{}'", xcc::fs::path::getFileName(filename));

    xcc::compile(globalContext, xcc::FileManager::load(filename), false, args.mod_paths);
  }

  globalContext->flushModulesToJIT();
  globalContext->runFunction("main");

  return 0;
}

static int repl(std::unique_ptr<xcc::codegen::GlobalContext> globalContext, xcc::args::Arguments& args) {
  logger.setEnable(true);

  logger.print("xcc (experimental) repl {}\n", xcc::getVersion());

  size_t input_counter = 0;

  while (true) {
    // TODO: History, arrow key parser
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

    xcc::run(globalContext, xcc::FileManager::createVirtual("repl_input_" + std::to_string(input_counter++), line), true);
  }

  return 0;
}

static int xcc_main(int argc, char ** argv) {
  auto args = xcc::args::parse(argc, argv);

  if (std::find(args.loggers.begin(), args.loggers.end(), "*") != args.loggers.end()) {
    args.loggers = xcc::util::log::getModuleNames();
  }

  for (auto& mod : args.loggers) {
    xcc::util::log::enableModule(mod, true);
  }

  if (args.help) {
    return help();
  }

  if (args.version) {
    return version();
  }

  if (args.target == "list") {
    return list_targets(args);
  }

  if (args.machine == "list") {
    return list_machines(args);
  }

  if (args.compile && args.run) {
    logger.error("--run cannot be used with --compile");
    return 1;
  }

  auto target = xcc::init(args.target, args.machine);

  auto globalContext = xcc::codegen::GlobalContext::create(target);

  if (!args.files.empty()) {
    if (args.run) {
      return run(std::move(globalContext), args);
    }

    if (args.compile) {
      return compile(std::move(globalContext), args);
    }

    return link(std::move(globalContext), args);
  }

  return repl(std::move(globalContext), args);
}

#undef USE_CATCH_EXCEPTIONS
#define USE_CATCH_EXCEPTIONS 0

int main(int argc, char ** argv) {
#if USE_CATCH_EXCEPTIONS
  try {
#endif
    return xcc_main(argc, argv);
#if USE_CATCH_EXCEPTIONS
  } catch (std::exception& e) {
    fprintf(stderr, "%s\n", e.what());
    return 1;
  }
#endif
}

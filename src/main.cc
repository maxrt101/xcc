#include <iostream>
#include <fstream>
#include <sstream>

#include "xcc/xcc.h"
#include "xcc/args.h"
#include "xcc/util/string.h"
#include "xcc/util/env.h"
#include "xcc/util/fs.h"

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
  logger.println("XCC Compiler v{}", xcc::getVersion());
  logger.println("Usage: xcc [-h] [-v] [--verbose] [-c] [-r] [-l LIB] [-L PATH] [-I PATH] [-t TARGET] [-m MACHINE] [-o OUT_FILE] IN_FILE...");
  logger.println("Arguments:");
  logger.println("  -h, --help              - Print this message");
  logger.println("  -v, --version           - Print version");
  logger.println("  -c, --compile           - Compile into object file");
  logger.println("  -r, --run               - Run file using JIT");
  logger.println("  -l, --lib LIB           - Link LIB");
  logger.println("  -L, --lib-path LIB_PATH - Add library search path");
  logger.println("  -I, --mod-path MOD_PATH - Add module search path");
  logger.println("  -t, --target TARGET     - Specify target triple (use 'list' to see all)");
  logger.println("  -m, --machine MACHINE   - Specify target machine (cpu) (use 'list' to see all)");
  logger.println("  -o, --output OUT_FILE   - Set output file name");
  logger.println("  IN_FILE...              - Input (source/object) files");
  logger.println("Environment:");
  logger.println("  XCC_LD                  - Path to linker executable");
  logger.println("  XCC_LDFLAGS             - Flags to pass directly to linker");
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
    targets.push_back({t.getName(), t.getShortDescription()});
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
  auto src = xcc::fs::readFile(filename);
  auto out = args.output.empty() ? filename + ".o" : args.output;

  logger.info("Compiling '{}' into '{}'", filename, out);

  xcc::compile_to_object(globalContext, src, out, getModPaths(filename, args));

  return 0;
}

static int link(std::unique_ptr<xcc::codegen::GlobalContext> globalContext, xcc::args::Arguments& args) {
  std::vector<std::string> files_to_link;

  for (auto & filename : args.files) {
    auto file = xcc::fs::readFile(filename);

    auto magic = llvm::identify_magic(file);

    if (magic.is_object()) {
      logger.info("Found object file '{}'", xcc::fs::path::getFileName(filename));
      files_to_link.push_back(filename);
    } else {
      auto out = filename + ".o";

      logger.info("Compiling '{}' into '{}'", xcc::fs::path::getFileName(filename), xcc::fs::path::getFileName(out));

      xcc::compile_to_object(globalContext, file, out, getModPaths(filename, args));

      auto target = globalContext->target;
      globalContext = xcc::codegen::GlobalContext::create();
      globalContext->setTarget(target);
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

    xcc::compile(globalContext, xcc::fs::readFile(filename), false, args.mod_paths);
  }

  globalContext->flushModulesToJIT();
  globalContext->runFunction("main");

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

    xcc::run(globalContext, line, true);
  }

  return 0;
}

static int xcc_main(int argc, char ** argv) {
  auto args = xcc::args::parse(argc, argv);

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

  auto globalContext = xcc::codegen::GlobalContext::create();

  globalContext->setTarget(target);

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

int main(int argc, char ** argv) {
#if USE_CATCH_EXCEPTIONS
  try {
#endif
    return xcc_main(argc, argv);
#if USE_CATCH_EXCEPTIONS
  } catch (std::exception& e) {
    logger.error("{}\n", e.what());
  }
#endif
}

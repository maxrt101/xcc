#include "xcc/args.h"
#include <stdexcept>
#include <cstring>
#include <llvm/TargetParser/Host.h>

#define CHECK_ARG(__i, __argc, __key) \
  if (__i + 1 >= argc) throw std::runtime_error("Expected value for key '" __key "'");

xcc::args::Arguments::Arguments()
  : help(false),
    version(false),
    verbose(false),
    compile(false),
    run(false),
    log_stderr(false),
    target(llvm::sys::getDefaultTargetTriple()),
    machine("generic") {}

xcc::args::Arguments xcc::args::parse(int argc, char ** argv) {
  Arguments args;

  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
      args.help = true;
    } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) {
      args.version = true;
    } else if (!strcmp(argv[i], "--verbose")) {
      args.verbose = true;
    } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--compile")) {
      args.compile = true;
    } else if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "--run")) {
      args.run = true;
    } else if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--lib")) {
      CHECK_ARG(i, argc, "lib");
      args.libs.emplace_back(argv[++i]);
    } else if (!strcmp(argv[i], "-L") || !strcmp(argv[i], "--lib-path")) {
      CHECK_ARG(i, argc, "lib_path");
      args.lib_paths.emplace_back(argv[++i]);
    } else if (!strcmp(argv[i], "-I") || !strcmp(argv[i], "--mod-path")) {
      CHECK_ARG(i, argc, "mod_path");
      args.mod_paths.emplace_back(argv[++i]);
    } else if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--target")) {
      CHECK_ARG(i, argc, "target");
      args.target = argv[++i];
    } else if (!strcmp(argv[i], "-m") || !strcmp(argv[i], "--machine")) {
      CHECK_ARG(i, argc, "machine");
      args.machine = argv[++i];
    } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
      CHECK_ARG(i, argc, "output");
      args.output = argv[++i];
    } else if (!strcmp(argv[i], "--log")) {
      CHECK_ARG(i, argc, "output");
      args.loggers.emplace_back(argv[++i]);
    } else if (!strcmp(argv[i], "--log-stderr")) {
      args.log_stderr = true;
    } else if (!strcmp(argv[i], "--log-file")) {
      CHECK_ARG(i, argc, "log_file");
      args.log_files.emplace_back(argv[++i]);
    } else {
      args.files.emplace_back(argv[i]);
    }
  }

  return args;
}

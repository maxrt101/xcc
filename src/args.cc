#include "xcc/args.h"
#include <exception>
#include <cstring>
#include <llvm/TargetParser/Host.h>

#define CHECK_ARG(__i, __argc, __key) \
  if (__i + 1 >= argc) throw std::runtime_error("Expected value for key '" __key "'");

xcc::args::Arguments::Arguments()
  : help(false),
    version(false),
    verbose(false),
    compile(false),
    link(false),
    run(false),
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
    } else if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--link")) {
      args.link = true;
    } else if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "--run")) {
      args.run = true;
    } else if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--target")) {
      CHECK_ARG(i, argc, "target");
      args.target = argv[++i];
    } else if (!strcmp(argv[i], "-m") || !strcmp(argv[i], "--machine")) {
      CHECK_ARG(i, argc, "machine");
      args.machine = argv[++i];
    } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
      CHECK_ARG(i, argc, "output");
      args.output = argv[++i];
    } else {
      args.files.push_back(argv[i]);
    }
  }

  return args;
}

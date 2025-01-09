#include <iostream>
#include <fstream>
#include <sstream>

#include "xcc/xcc.h"
#include "xcc/util/string.h"

static auto logger = xcc::util::log::Logger("MAIN");

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

int main(int argc, char ** argv) {
  xcc::init();

  auto globalContext = xcc::codegen::GlobalContext::create();

  if (argc == 2) {
    std::ifstream fs(argv[1]);
    std::stringstream ss;
    ss << fs.rdbuf();

    try {
      xcc::run(globalContext, ss.str(), false);
    } catch (std::exception& e) {
      logger.fatal("%s", e.what());
      return 1;
    }

    return 0;
  }

  logger.print("xcc (experimental) repl %s by maxrt\n", xcc::getVersion().c_str());

  while (true) {
    logger.print("-> ");

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

    if (tokens[0] == "/quit" || tokens[0] == "/q") {
      break;
    }

    if (tokens[0] == "/help" || tokens[0] == "/h") {
      logger.print("/help or /h - Prints this message\n");
      logger.print("/quit or /q - Exits from REPL\n");
      logger.print("/list or /l - List global function symbols\n");
      continue;
    }

    if (tokens[0] == "/list") {
      for (auto& [name, fn] : globalContext->functions) {
        logger.print("%s\n", fn->toString().c_str());
      }
      continue;
    }

    try {
      xcc::run(globalContext, line, true);
    } catch (std::exception& e) {
      logger.error("%s\n", e.what());
    }
  }

  return 0;
}

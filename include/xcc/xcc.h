#pragma once

#include <cstdint>
#include <string>
#include "xcc/lexer.h"
#include "xcc/parser.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/util/log.h"

namespace xcc {

constexpr uint8_t XCC_VERSION_MAJOR = 0;
constexpr uint8_t XCC_VERSION_MINOR = 1;
constexpr uint8_t XCC_VERSION_PATCH = 0;

inline std::string getVersion() {
  return std::to_string(XCC_VERSION_MAJOR) + "."
       + std::to_string(XCC_VERSION_MINOR) + "."
       + std::to_string(XCC_VERSION_PATCH);
}

void init(bool autoCleanup = true);

void cleanup();

void run(std::unique_ptr<codegen::GlobalContext>& globalContext, const std::string& src, bool isRepl = false);

}

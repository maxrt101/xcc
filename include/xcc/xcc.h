#pragma once

#include <cstdint>
#include <string>

namespace xcc {

constexpr uint8_t XCC_VERSION_MAJOR = 0;
constexpr uint8_t XCC_VERSION_MINOR = 1;
constexpr uint8_t XCC_VERSION_PATCH = 0;

inline std::string getVersion() {
  return std::to_string(XCC_VERSION_MAJOR) + "."
       + std::to_string(XCC_VERSION_MINOR) + "."
       + std::to_string(XCC_VERSION_PATCH);
}

}

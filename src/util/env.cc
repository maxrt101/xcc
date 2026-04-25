#include "xcc/util/env.h"
#include <cstdlib>

std::string xcc::env::get(const std::string& name, const std::string& default_value) {
  char * value = getenv(name.c_str());

  if (value) {
    return value;
  }

  return default_value;
}

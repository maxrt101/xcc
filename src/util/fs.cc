#include "xcc/util/fs.h"
#include "xcc/util/log.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>

static auto logger = xcc::util::log::Logger("FS");

std::string xcc::fs::readFile(const std::string& filename) {
  std::ifstream fs(filename);

  if (!fs.is_open()) {
    logger.fatal("Failed to open file '{}'", filename);
    throw std::runtime_error("Failed to open the file");
  }

  std::stringstream ss;
  ss << fs.rdbuf();

  return ss.str();
}

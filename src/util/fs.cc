#include "xcc/util/fs.h"
#include "xcc/util/log.h"
#include "xcc/util/string.h"
#include "xcc/error.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <filesystem>

static auto logger = xcc::util::log::Logger("FS");

std::string xcc::fs::readFile(const std::string& filename) {
  std::ifstream fs(filename);

  if (!fs.is_open()) {
    logger.fatal("Failed to open file '{}'", filename);
    Error(ERROR_MISSING_FILE, {}, filename).throwException();
  }

  std::stringstream ss;
  ss << fs.rdbuf();

  return ss.str();
}

bool xcc::fs::exists(const std::string& path) {
  return std::filesystem::exists(path);
}

std::vector<std::string> xcc::fs::path::split(const std::string& path) {
  auto res = util::strsplit(path, "/");

  std::erase_if(res, [](auto& x) { return x.empty(); });

  return res;
}

std::string xcc::fs::path::getParent(const std::string& path) {
  auto pos = path.rfind("/");

  return pos == std::string::npos ? path : path.substr(0, pos);
}

std::string xcc::fs::path::getFileName(const std::string& path) {
  auto pos = path.rfind("/");

  return pos == std::string::npos ? path : path.substr(pos+1);
}

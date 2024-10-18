#include "xcc/util/string.h"

std::vector<std::string> xcc::util::strsplit(const std::string& str, const std::string& delimiter) {
  std::vector<std::string> result;
  size_t last = 0, next = 0;
  while ((next = str.find(delimiter, last)) != std::string::npos) {
    result.push_back(str.substr(last, next-last));
    last = next + 1;
  }
  result.push_back(str.substr(last));
  return result;
}
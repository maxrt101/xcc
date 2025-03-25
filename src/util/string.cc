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

std::string xcc::util::strjoin(const std::vector<std::string>& parts, const std::string& delimiter) {
  std::string result;

  for (size_t i = 0; i < parts.size(); ++i) {
    result += parts[i];
    if (i + 1 != parts.size()) {
      result += delimiter;
    }
  }

  return result;
}

void xcc::util::strreplace(std::string& str, const std::string& from, const std::string& to) {
  if (from.empty()) {
    return;
  }
  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();
  }
}

std::string xcc::util::strescseq(const std::string& str, bool add) {
  std::string result = str;

  // TODO: Make two-way map for this
  if (add) {
    util::strreplace(result, "\\n", "\n");
    util::strreplace(result, "\\r", "\r");
    util::strreplace(result, "\\t", "\t");
    util::strreplace(result, "\\b", "\b");
  } else {
    util::strreplace(result, "\n", "\\n");
    util::strreplace(result, "\r", "\\r");
    util::strreplace(result, "\t", "\\t");
    util::strreplace(result, "\b", "\\b");
  }

  return result;
}
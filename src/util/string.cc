#include "xcc/util/string.h"
#include <format>

bool xcc::util::contains(const std::string& str, const std::string& sub) {
  return str.find(sub) != std::string::npos;
}

std::vector<std::string> xcc::util::strsplit(const std::string& str, const std::string& delimiter) {
  std::vector<std::string> result;

  size_t last = 0, next = 0;

  while ((next = str.find(delimiter, last)) != std::string::npos) {
    auto sub = str.substr(last, next-last);

    if (!sub.empty()) {
      result.push_back(sub);
    }

    last = next + 1;
  }

  auto sub = str.substr(last);

  if (!sub.empty()) {
    result.push_back(sub);
  }

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

std::string xcc::util::toStringWithOrdinalSuffix(int num) {
  switch (num) {
    case 1: return "1st";
    case 2: return "2nd";
    case 3: return "3rd";
    default:
      return std::format("{}th", num);
  }
}

xcc::util::BaseDetermineResult xcc::util::determineBase(const std::string& value) {
  BaseDetermineResult res = {.base = 10, .value = value};

  int prefix_len = 2;

  if (value.size() > 2 && value[0] == '0') {
    if (value[1] == 'x') {
      res.base = 16;
    } else if (value[1] == 'b') {
      res.base = 2;
    } else if (value[1] == 'o') {
      res.base = 8;
    } else {
      res.base = 8;
      prefix_len = 1;
    }
    res.value = value.substr(prefix_len);
  }

  return res;
}

#pragma once

#include <string>
#include <vector>

namespace xcc::util {

std::vector<std::string> strsplit(const std::string& str, const std::string& delimiter = " ");

void strreplace(std::string& str, const std::string& from, const std::string& to);

std::string strescseq(const std::string& str, bool add = true);

}

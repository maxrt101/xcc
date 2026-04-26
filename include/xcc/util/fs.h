#pragma once

#include <string>
#include <vector>

namespace xcc::fs {

std::string readFile(const std::string& filename);

bool exists(const std::string& path);

namespace path {

std::vector<std::string> split(const std::string& path);

std::string getParent(const std::string& path);
std::string getFileName(const std::string& path);

}

} /* namespace xcc::fs */

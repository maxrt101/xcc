#pragma once

#include <string>

namespace xcc::env {

std::string get(const std::string& name, const std::string& default_value = "");

} /* namespace xcc::env */

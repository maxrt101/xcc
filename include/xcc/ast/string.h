#pragma once

#include "xcc/ast/node.h"

#include <string>

namespace xcc::ast {

class String : public Node {
public:
  std::string value;

public:
  explicit String(const std::string& value);
  virtual ~String() override = default;

  static std::shared_ptr<String> create(const std::string& value);
};

} /* namespace xcc::ast */

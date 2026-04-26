#pragma once

#include "xcc/ast/node.h"

namespace xcc::ast {

class Use : public Node {
public:
  std::shared_ptr<Node> name;

public:
  explicit Use(std::shared_ptr<Node> name);
  virtual ~Use() override = default;

  static std::shared_ptr<Use> create(std::shared_ptr<Node> name);
};

} /* namespace xcc::ast */

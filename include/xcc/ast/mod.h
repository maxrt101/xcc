#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/block.h"

namespace xcc::ast {

class Module : public Node {
public:
  std::shared_ptr<Node>  name;
  std::shared_ptr<Block> body;

public:
  explicit Module(std::shared_ptr<Node> name, std::shared_ptr<Block> body = nullptr);
  ~Module() override = default;

  static std::shared_ptr<Module> create(std::shared_ptr<Node> name, std::shared_ptr<Block> body = nullptr);
};

} /* namespace xcc::ast */

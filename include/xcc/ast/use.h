#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/block.h"

namespace xcc::ast {

class Use : public Node {
public:
  std::shared_ptr<Node>  name;
  std::shared_ptr<Block> items;

public:
  explicit Use(std::shared_ptr<Node> name, std::shared_ptr<Block> items = nullptr);
  virtual ~Use() override = default;

  static std::shared_ptr<Use> create(std::shared_ptr<Node> name, std::shared_ptr<Block> items = nullptr);
};

} /* namespace xcc::ast */

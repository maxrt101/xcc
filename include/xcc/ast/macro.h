#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/block.h"
#include "xcc/ast/identifier.h"

namespace xcc::ast {

class Macro : public Node {
public:
  std::shared_ptr<Identifier>              name;
  std::vector<std::shared_ptr<Identifier>> args;
  std::shared_ptr<Block>                   body;

public:
  Macro(
    std::shared_ptr<Identifier>              name,
    std::vector<std::shared_ptr<Identifier>> args,
    std::shared_ptr<Block>                   body
  );

  ~Macro() override = default;

  static std::shared_ptr<Macro> create(
    std::shared_ptr<Identifier>              name,
    std::vector<std::shared_ptr<Identifier>> args,
    std::shared_ptr<Block>                   body
  );

  std::shared_ptr<Node> clone() override;
  void visit(Visitor visitor) override;
};

} /* namespace xcc::ast */

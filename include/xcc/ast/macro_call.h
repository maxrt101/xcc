#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/identifier.h"

namespace xcc::ast {

class MacroCall : public Node {
public:
  std::shared_ptr<Identifier>        name;
  std::vector<std::shared_ptr<Node>> args;

public:
  MacroCall(
    std::shared_ptr<Identifier>        name,
    std::vector<std::shared_ptr<Node>> args
  );

  ~MacroCall() override = default;

  static std::shared_ptr<MacroCall> create(
    std::shared_ptr<Identifier>        name,
    std::vector<std::shared_ptr<Node>> args
  );

  std::shared_ptr<Node> clone() override;
  void visit(Visitor visitor) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;
};

} /* namespace xcc::ast */


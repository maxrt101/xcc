#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/block.h"

namespace xcc::ast {

class Module : public Node {
public:
  std::shared_ptr<Node>  name;
  std::shared_ptr<Block> body;

public:
  explicit Module(SourceSpan span, std::shared_ptr<Node> name, std::shared_ptr<Block> body = nullptr);
  ~Module() override = default;

  static std::shared_ptr<Module> create(SourceSpan span, std::shared_ptr<Node> name, std::shared_ptr<Block> body = nullptr);

  std::shared_ptr<Node> clone() override;
  void visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  std::string getName() const;
};

} /* namespace xcc::ast */

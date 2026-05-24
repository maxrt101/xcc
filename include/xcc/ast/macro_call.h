#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/identifier.h"

namespace xcc::ast {

class MacroCall : public Node {
public:
  std::shared_ptr<Identifier> name;
  NodeList                    args;

public:
  MacroCall(
    SourceSpan                  span,
    LexicalScope                scope,
    std::shared_ptr<Identifier> name,
    NodeList                    args
  );

  ~MacroCall() override = default;

  static std::shared_ptr<MacroCall> create(
    SourceSpan                  span,
    LexicalScope                scope,
    std::shared_ptr<Identifier> name,
    NodeList                    args
  );

  std::shared_ptr<Node> clone() override;
  void visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;
};

} /* namespace xcc::ast */


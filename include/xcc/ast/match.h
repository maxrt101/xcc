#pragma once

#include "xcc/ast/node.h"

#include <string>
#include <vector>

namespace xcc::ast {

class Match : public Node {
public:
  struct Pattern {
    NodeList nodes;
  };

  struct Arm {
    Pattern               pattern;
    std::shared_ptr<Node> then;
  };

  std::vector<Arm>      arms;
  std::shared_ptr<Node> value;

public:
  Match(
    SourceSpan            span,
    std::shared_ptr<Node> value,
    std::vector<Arm>      arms
  );

  ~Match() override = default;

  static std::shared_ptr<Match> create(
    SourceSpan            span,
    std::shared_ptr<Node> value,
    std::vector<Arm>      arms
  );

  std::shared_ptr<Node> clone() override;
  void visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;

private:
  std::shared_ptr<Node> findOrCreateDefault();
};

} /* namespace xcc::ast */

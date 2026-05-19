#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/type.h"

namespace xcc::ast {

class Asm : public Node {
public:
  std::shared_ptr<Node> code;
  std::shared_ptr<Node> constraints;
  NodeList              args;

public:
  Asm(
    SourceSpan            span,
    std::shared_ptr<Node> code,
    std::shared_ptr<Node> constraints,
    NodeList args
  );

  ~Asm() override = default;

  static std::shared_ptr<Asm> create(
    SourceSpan            span,
    std::shared_ptr<Node> code,
    std::shared_ptr<Node> constraints,
    NodeList              args
  );

  std::shared_ptr<Node> clone() override;
  void visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

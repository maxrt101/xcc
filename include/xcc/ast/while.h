#pragma once

#include "xcc/ast/node.h"

namespace xcc::ast {

class While : public Node {
public:
  std::shared_ptr<Node> condition;
  std::shared_ptr<Node> body;

public:
  While(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> condition, std::shared_ptr<Node> body);
  ~While() override = default;

  static std::shared_ptr<While> create(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> condition, std::shared_ptr<Node> body);

  std::shared_ptr<Node> clone() override;
  void visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/block.h"

namespace xcc::ast {

class If : public Node {
public:
  std::shared_ptr<Node> condition;
  std::shared_ptr<Node> then_branch;
  std::shared_ptr<Node> else_branch;

public:
  If(SourceSpan span, std::shared_ptr<Node> condition, std::shared_ptr<Node> then_branch, std::shared_ptr<Node> else_branch = nullptr);
  ~If() override = default;

  static std::shared_ptr<If> create(SourceSpan span, std::shared_ptr<Node> condition, std::shared_ptr<Node> then_branch, std::shared_ptr<Node> else_branch = nullptr);

  std::shared_ptr<Node> clone() override;
  void visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

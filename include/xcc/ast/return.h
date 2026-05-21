#pragma once

#include "xcc/ast/node.h"

namespace xcc::ast {

class Return : public Node {
public:
  std::shared_ptr<Node> value;

public:
  explicit Return(SourceSpan span, std::shared_ptr<Node> value = nullptr);
  ~Return() override = default;

  static std::shared_ptr<Return> create(SourceSpan span, std::shared_ptr<Node> value = nullptr);

  std::shared_ptr<Node> clone() override;
  void visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

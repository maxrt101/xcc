#pragma once

#include "xcc/ast/node.h"
#include "xcc/lexer.h"

namespace xcc::ast {

class Binary : public Node {
public:
  Token operation;
  std::shared_ptr<Node> lhs, rhs;

public:
  Binary(Token operation, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);
  ~Binary() override = default;

  static std::shared_ptr<Binary> create(Token operation, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);

  std::shared_ptr<Node> clone() override;
  void visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

#pragma once

#include "xcc/ast/node.h"
#include "xcc/lexer.h"

namespace xcc::ast {

class Assign : public Node {
public:
  Token kind;
  std::shared_ptr<Node> lhs;
  std::shared_ptr<Node> rhs;

public:
  Assign(SourceSpan span, Token kind, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);
  ~Assign() override = default;

  static std::shared_ptr<Assign> create(SourceSpan span, Token kind, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);

  std::shared_ptr<Node> clone() override;
  void visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

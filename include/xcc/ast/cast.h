#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/type.h"

namespace xcc::ast {

class Cast : public Node {
public:
  std::shared_ptr<Node> expr;
  std::shared_ptr<Node> type;

public:
  Cast(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> expr, std::shared_ptr<Node> type);
  ~Cast() override = default;

  static std::shared_ptr<Cast> create(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> expr, std::shared_ptr<Node> type);

  std::shared_ptr<Node> clone() override;
  void visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/vardecl.h"

namespace xcc::ast {

class For : public Node {
public:
  std::shared_ptr<VarDecl> init;
  std::shared_ptr<Node>    cond;
  std::shared_ptr<Node>    step;
  std::shared_ptr<Node>    body;

public:
  For(SourceSpan span, std::shared_ptr<VarDecl> init, std::shared_ptr<Node> cond, std::shared_ptr<Node> step, std::shared_ptr<Node> body);
  ~For() override = default;

  static std::shared_ptr<For> create(SourceSpan span, std::shared_ptr<VarDecl> init, std::shared_ptr<Node> cond, std::shared_ptr<Node> step, std::shared_ptr<Node> body);

  std::shared_ptr<Node> clone() override;
  void visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

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

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;

  std::shared_ptr<Node> expand(codegen::ModuleContext& ctx, PayloadList payload);
};

} /* namespace xcc::ast */


#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/type.h"
#include "xcc/ast/identifier.h"

#include <string>
#include <vector>

namespace xcc::ast {

class TypedIdentifier : public Node {
public:
  std::shared_ptr<Identifier> name;
  std::shared_ptr<Node>       value_type;
  std::shared_ptr<Node>       value;

public:
  TypedIdentifier(SourceSpan span, std::shared_ptr<Identifier> name, std::shared_ptr<Node> type, std::shared_ptr<Node> value = nullptr);
  ~TypedIdentifier() override = default;

  static std::shared_ptr<TypedIdentifier> create(SourceSpan span, std::shared_ptr<Identifier> name, std::shared_ptr<Node> type, std::shared_ptr<Node> value = nullptr);

  std::shared_ptr<Node> clone() override;
  void visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext &ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

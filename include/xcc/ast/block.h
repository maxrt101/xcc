#pragma once

#include "xcc/ast/node.h"

#include <string>
#include <vector>

namespace xcc::ast {

class Block : public Node {
public:
  struct Payload : Node::Payload {
    std::shared_ptr<meta::Type> type;

    explicit Payload(std::shared_ptr<meta::Type> type);
    ~Payload() override = default;

    static std::shared_ptr<Node::Payload> create(std::shared_ptr<meta::Type> type);
  };

public:
  std::vector<std::shared_ptr<Node>> body;

public:
  explicit Block(SourceSpan span, std::vector<std::shared_ptr<Node>> body);
  ~Block() override = default;

  static std::shared_ptr<Block> create(SourceSpan span, std::vector<std::shared_ptr<Node>> body);

  std::shared_ptr<Node> clone() override;
  void visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValue(codegen::ModuleContext &ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

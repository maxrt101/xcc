#pragma once

#include "xcc/ast/node.h"

#include <string>
#include <vector>

namespace xcc::ast {

class Block : public Node {
public:
  struct Payload : Node::Payload {
    std::shared_ptr<meta::Type> type;
    bool                        is_fn_block;

    explicit Payload(std::shared_ptr<meta::Type> type, bool is_fn_block = false);
    ~Payload() override = default;

    static std::shared_ptr<Node::Payload> create(std::shared_ptr<meta::Type> type, bool is_fn_block = false);
  };

public:
  NodeList body;
  bool     has_result;

public:
  explicit Block(SourceSpan span, LexicalScope scope, NodeList body, bool has_result = false);
  ~Block() override = default;

  static std::shared_ptr<Block> create(SourceSpan span, LexicalScope scope, NodeList body, bool has_result = false);

  std::shared_ptr<Node> clone() override;
  void visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValue(codegen::ModuleContext &ctx, PayloadList payload) override;
  llvm::Constant * generateConstant(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

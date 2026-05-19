#pragma once

#include "xcc/ast/node.h"

namespace xcc::ast {

class Number : public Node {
public:
  struct Payload : Node::Payload {
    int bits;

    explicit Payload(int bits);
    ~Payload() override = default;

    static std::shared_ptr<Node::Payload> create(int bits);
  };

public:
  enum {
    INTEGER,
    FLOATING
  } tag;

  union {
    int64_t integer;
    double floating;
  } value;

public:
  Number(SourceSpan span);
  ~Number() override = default;

  static std::shared_ptr<Number> createInteger(SourceSpan span, int64_t value);
  static std::shared_ptr<Number> createFloating(SourceSpan span, double value);

  std::shared_ptr<Node> clone() override;
  void visit(std::unique_ptr<codegen::GlobalContext>& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  llvm::Value * generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) override;
  llvm::Constant * generateConstant(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

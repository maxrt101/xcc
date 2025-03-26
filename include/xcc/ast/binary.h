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
  virtual ~Binary() override = default;

  static std::shared_ptr<Binary> create(Token operation, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

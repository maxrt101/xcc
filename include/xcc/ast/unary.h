#pragma once

#include "xcc/ast/node.h"
#include "xcc/lexer.h"

#include <string>

namespace xcc::ast {

class Unary : public Node {
public:
  Token operation;
  std::shared_ptr<Node> rhs;

public:
  Unary(Token operation, std::shared_ptr<Node> rhs);
  ~Unary() override = default;

  static std::shared_ptr<Unary> create(Token operation, std::shared_ptr<Node> rhs);

  std::shared_ptr<Node> clone() override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  llvm::Value * generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<xcc::meta::Type> generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

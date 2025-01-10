#pragma once

#include "xcc/ast/node.h"

namespace xcc::ast {

class Return : public Node {
public:
  std::shared_ptr<Node> value;

public:
  explicit Return(std::shared_ptr<Node> value = nullptr);
  virtual ~Return() override = default;

  static std::shared_ptr<Return> create(std::shared_ptr<Node> value = nullptr);

  llvm::Value * generateValue(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
};

} /* namespace xcc::ast */

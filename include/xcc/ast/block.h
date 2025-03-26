#pragma once

#include "xcc/ast/node.h"

#include <string>
#include <vector>

namespace xcc::ast {

class Block : public Node {
public:
  std::vector<std::shared_ptr<Node>> body;

public:
  explicit Block(std::vector<std::shared_ptr<Node>> body);
  virtual ~Block() override = default;

  static std::shared_ptr<Block> create(std::vector<std::shared_ptr<Node>> body);

  llvm::Value * generateValue(codegen::ModuleContext &ctx, PayloadList payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

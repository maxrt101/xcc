#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/block.h"

namespace xcc::ast {

class If : public Node {
public:
  std::shared_ptr<Node> condition;
  std::shared_ptr<Node> then_branch;
  std::shared_ptr<Node> else_branch;

public:
  If(std::shared_ptr<Node> condition, std::shared_ptr<Node> then_branch, std::shared_ptr<Node> else_branch = nullptr);
  virtual ~If() override = default;

  static std::shared_ptr<If> create(std::shared_ptr<Node> condition, std::shared_ptr<Node> then_branch, std::shared_ptr<Node> else_branch = nullptr);

  llvm::Value * generateValue(codegen::ModuleContext& ctx, void * payload = nullptr) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, void * payload = nullptr) override;
};

} /* namespace xcc::ast */

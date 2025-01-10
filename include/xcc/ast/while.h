#pragma once

#include "xcc/ast/node.h"

namespace xcc::ast {

class While : public Node {
public:
  std::shared_ptr<Node> condition;
  std::shared_ptr<Node> body;

public:
  While(std::shared_ptr<Node> condition, std::shared_ptr<Node> body);
  virtual ~While() override = default;

  static std::shared_ptr<While> create(std::shared_ptr<Node> condition, std::shared_ptr<Node> body);

  llvm::Value * generateValue(codegen::ModuleContext& ctx, void * payload = nullptr) override;
};

} /* namespace xcc::ast */

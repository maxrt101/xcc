#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/type.h"
#include "xcc/ast/identifier.h"

#include <string>
#include <vector>

namespace xcc::ast {

class Assign : public Node {
public:
  std::shared_ptr<Node> lhs;
  std::shared_ptr<Node> rhs;

public:
  Assign(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);
  virtual ~Assign() override = default;

  static std::shared_ptr<Assign> create(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);

  llvm::Value * generateValue(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
};

} /* namespace xcc::ast */

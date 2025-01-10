#pragma once

#include "xcc/ast/node.h"
#include "xcc/lexer.h"

#include <string>

namespace xcc::ast {

class Subscript : public Node {
public:
  std::shared_ptr<Node> lhs, rhs;

public:
  Subscript(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);
  virtual ~Subscript() override = default;

  static std::shared_ptr<Subscript> create(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);

  llvm::Value * generateValue(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
  llvm::Value * generateValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
  std::shared_ptr<xcc::meta::Type> generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
};

} /* namespace xcc::ast */

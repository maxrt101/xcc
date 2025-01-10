#pragma once

#include "xcc/ast/node.h"

#include <string>

namespace xcc::ast {

class String : public Node {
public:
  std::string value;

public:
  explicit String(std::string value);
  virtual ~String() override = default;

  static std::shared_ptr<String> create(std::string value);

  llvm::Value * generateValue(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
  llvm::Value * generateValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
};

} /* namespace xcc::ast */

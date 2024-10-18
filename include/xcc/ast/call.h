#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/typed_identifier.h"

#include <string>
#include <vector>

namespace xcc::ast {

class Call : public Node {
public:
  std::shared_ptr<Node> name;
  // Generics?
  std::vector<std::shared_ptr<Node>> args;

public:
  Call(std::shared_ptr<Node> name, std::vector<std::shared_ptr<Node>> args);
  virtual ~Call() override = default;

  static std::shared_ptr<Call> create(std::shared_ptr<Node> name, std::vector<std::shared_ptr<Node>> args);

  llvm::Value * generateValue(codegen::ModuleContext& ctx) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx) override;
};

} /* namespace xcc::ast */

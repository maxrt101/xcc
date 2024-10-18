#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/type.h"
#include "xcc/ast/identifier.h"

#include <string>
#include <vector>

namespace xcc::ast {

class Assign : public Node {
public:
  std::shared_ptr<Identifier> name;
  std::shared_ptr<Node> value;

public:
  Assign(std::shared_ptr<Identifier> name, std::shared_ptr<Node> value);
  virtual ~Assign() override = default;

  static std::shared_ptr<Assign> create(std::shared_ptr<Identifier> name, std::shared_ptr<Node> value);

  llvm::Value * generateValue(codegen::ModuleContext& ctx) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx) override;
};

} /* namespace xcc::ast */

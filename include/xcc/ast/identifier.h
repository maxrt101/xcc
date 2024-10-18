#pragma once

#include "xcc/ast/node.h"

#include <string>

namespace xcc::ast {

class Identifier : public Node {
public:
  std::string value;

public:
  explicit Identifier(const std::string& value);
  virtual ~Identifier() override = default;

  static std::shared_ptr<Identifier> create(const std::string& value);

  llvm::Value * generateValue(codegen::ModuleContext& ctx) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx) override;
};

} /* namespace xcc::ast */

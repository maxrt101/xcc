#pragma once

#include "xcc/ast/node.h"

#include <string>

namespace xcc::ast {

class Identifier : public Node {
public:
  std::string value;

public:
  explicit Identifier(std::string value);
  virtual ~Identifier() override = default;

  static std::shared_ptr<Identifier> create(const std::string& value);

  llvm::Value * generateValue(codegen::ModuleContext& ctx, void * payload = nullptr) override;
  llvm::Value * generateValueWithoutLoad(codegen::ModuleContext& ctx, void * payload = nullptr) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, void * payload = nullptr) override;
  std::shared_ptr<xcc::meta::Type> generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, void * payload = nullptr) override;
};

} /* namespace xcc::ast */

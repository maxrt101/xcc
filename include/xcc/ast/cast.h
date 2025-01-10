#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/type.h"

#include <string>

namespace xcc::ast {

class Cast : public Node {
public:
  std::shared_ptr<Node> expr;
  std::shared_ptr<Type> type;

public:
  Cast(std::shared_ptr<Node> expr, std::shared_ptr<Type> type);
  virtual ~Cast() override = default;

  static std::shared_ptr<Cast> create(std::shared_ptr<Node> expr, std::shared_ptr<Type> type);

  llvm::Value * generateValue(codegen::ModuleContext& ctx, void * payload = nullptr) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, void * payload = nullptr) override;
};

} /* namespace xcc::ast */

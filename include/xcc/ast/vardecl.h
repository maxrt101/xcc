#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/type.h"
#include "xcc/ast/identifier.h"

#include <string>
#include <vector>

namespace xcc::ast {

class VarDecl : public Node {
public:
  std::shared_ptr<Identifier> name;
  std::shared_ptr<Type> type;
  std::shared_ptr<Node> value;
  bool global;

public:
  VarDecl(
      std::shared_ptr<Identifier> name,
      std::shared_ptr<Type> type,
      std::shared_ptr<Node> value = nullptr,
      bool global = false
  );

  virtual ~VarDecl() override = default;

  static std::shared_ptr<VarDecl> create(
      std::shared_ptr<Identifier> name,
      std::shared_ptr<Type> type,
      std::shared_ptr<Node> value = nullptr,
      bool global = false
  );

  llvm::Value * generateValue(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
};

} /* namespace xcc::ast */

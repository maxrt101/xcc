#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/type.h"
#include "xcc/ast/identifier.h"

#include <string>
#include <vector>

namespace xcc::ast {

class TypedIdentifier : public Node {
public:
  std::shared_ptr<Identifier> name;
  std::shared_ptr<Type> value_type;
  std::shared_ptr<Node> value;

public:
  TypedIdentifier(std::shared_ptr<Identifier> name, std::shared_ptr<Type> type, std::shared_ptr<Node> value = nullptr);
  virtual ~TypedIdentifier() override = default;

  static std::shared_ptr<TypedIdentifier> create(std::shared_ptr<Identifier> name, std::shared_ptr<Type> type, std::shared_ptr<Node> value = nullptr);

  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext &ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
};

} /* namespace xcc::ast */

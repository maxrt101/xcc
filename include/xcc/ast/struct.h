#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/typed_identifier.h"

#include <string>
#include <vector>

namespace xcc::ast {

class Struct : public Node, public std::enable_shared_from_this<Struct> {
public:
  std::shared_ptr<Identifier> name;
  std::vector<std::shared_ptr<TypedIdentifier>> fields;

public:
  Struct(
      std::shared_ptr<Identifier> name,
      std::vector<std::shared_ptr<TypedIdentifier>> fields = {}
  );

  virtual ~Struct() override = default;

  static std::shared_ptr<Struct> create(
      std::shared_ptr<Identifier> name,
      std::vector<std::shared_ptr<TypedIdentifier>> fields = {}
  );

  std::shared_ptr<xcc::meta::Type> generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext &ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
};

} /* namespace xcc::ast */

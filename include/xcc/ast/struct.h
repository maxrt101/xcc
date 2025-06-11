#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/fndef.h"
#include "xcc/ast/typed_identifier.h"

#include <string>
#include <vector>

namespace xcc::ast {

class Struct : public Node, public std::enable_shared_from_this<Struct> {
public:
  std::shared_ptr<Identifier> name;
  std::vector<std::shared_ptr<TypedIdentifier>> fields;
  std::vector<std::shared_ptr<FnDef>> methods;

public:
  explicit Struct(
      std::shared_ptr<Identifier> name,
      std::vector<std::shared_ptr<TypedIdentifier>> fields = {},
      std::vector<std::shared_ptr<FnDef>> methods = {}
  );

  ~Struct() override = default;

  static std::shared_ptr<Struct> create(
      std::shared_ptr<Identifier> name,
      std::vector<std::shared_ptr<TypedIdentifier>> fields = {},
      std::vector<std::shared_ptr<FnDef>> methods = {}
  );

  std::shared_ptr<xcc::meta::Type> generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext &ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

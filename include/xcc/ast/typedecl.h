#pragma once

#include "xcc/ast/node.h"

namespace xcc::ast {

class TypeDecl : public Node {
public:
  std::shared_ptr<Node> name;
  std::shared_ptr<Node> value;

public:
  explicit TypeDecl(std::shared_ptr<Node> name, std::shared_ptr<Node> value);
  virtual ~TypeDecl() override = default;

  static std::shared_ptr<TypeDecl> create(std::shared_ptr<Node> name, std::shared_ptr<Node> value);

  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

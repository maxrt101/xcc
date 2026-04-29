#pragma once

#include "xcc/ast/node.h"

namespace xcc::ast {

class TypeDecl : public Node {
public:
  std::shared_ptr<Node> name;
  std::shared_ptr<Node> value;

public:
  explicit TypeDecl(std::shared_ptr<Node> name, std::shared_ptr<Node> value);
  ~TypeDecl() override = default;

  static std::shared_ptr<TypeDecl> create(std::shared_ptr<Node> name, std::shared_ptr<Node> value);

  std::shared_ptr<Node> clone() override;

  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

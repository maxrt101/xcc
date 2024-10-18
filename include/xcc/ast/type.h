#pragma once

#include "xcc/ast/node.h"

#include <string>
#include <vector>

namespace xcc::ast {

class Type : public Node {
public:
  std::shared_ptr<Node> name;
  bool pointer;

public:
  explicit Type(std::shared_ptr<Node> name, bool pointer);
  virtual ~Type() override = default;

  static std::shared_ptr<Type> create(std::shared_ptr<Node> name, bool pointer);

  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx) override;
};

} /* namespace xcc::ast */

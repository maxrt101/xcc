#pragma once

#include "xcc/ast/node.h"

#include <string>
#include <vector>

namespace xcc::ast {

class Type : public Node {
public:
  std::shared_ptr<Node> name;
  bool                  pointer;

  std::shared_ptr<Type>              returnType;
  std::vector<std::shared_ptr<Type>> args;
  bool                               function;
  bool                               isVariadic;

public:
  explicit Type(std::shared_ptr<Node> name, bool pointer);
  Type(std::shared_ptr<Type> returnType, std::vector<std::shared_ptr<Type>> args, bool isVariadic);
  ~Type() override = default;

  static std::shared_ptr<Type> create(std::shared_ptr<Node> name, bool pointer = false);
  static std::shared_ptr<Type> createFunction(std::shared_ptr<Type> returnType, std::vector<std::shared_ptr<Type>> args, bool isVariadic = false);

  std::shared_ptr<Node> clone() override;
  void visit(Visitor visitor) override;

  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/typed_identifier.h"

#include <string>
#include <vector>

namespace xcc::ast {

class FnDecl : public Node, public std::enable_shared_from_this<FnDecl> {
public:
  std::shared_ptr<Identifier> name;
  std::shared_ptr<Node> return_type;
  std::vector<std::shared_ptr<TypedIdentifier>> args;
  bool isExtern;
  bool isVariadic;
  std::string alias_to;

public:
  FnDecl(
      std::shared_ptr<Identifier> name,
      std::shared_ptr<Node> return_type,
      std::vector<std::shared_ptr<TypedIdentifier>> args = {},
      bool isExtern = false,
      bool isVariadic = false
  );

  ~FnDecl() override = default;

  static std::shared_ptr<FnDecl> create(
      std::shared_ptr<Identifier> name,
      std::shared_ptr<Node> return_type,
      std::vector<std::shared_ptr<TypedIdentifier>> args = {},
      bool isExtern = false,
      bool isVariadic = false
  );

  std::shared_ptr<Node> clone() override;
  void visit(Visitor visitor) override;

  llvm::Function * generateFunction(codegen::ModuleContext& ctx, PayloadList payload) override;

  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

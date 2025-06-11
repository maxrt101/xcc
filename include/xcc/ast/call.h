#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/typed_identifier.h"

#include <string>
#include <vector>

namespace xcc::ast {

class Call : public Node {
public:
  std::shared_ptr<Node> callee;
  // Generics?
  std::vector<std::shared_ptr<Node>> args;

public:
  Call(std::shared_ptr<Node> callee, std::vector<std::shared_ptr<Node>> args);
  virtual ~Call() override = default;

  static std::shared_ptr<Call> create(std::shared_ptr<Node> callee, std::vector<std::shared_ptr<Node>> args);

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;

private:
  void getInfoFromCallee(codegen::ModuleContext& ctx, std::string * name, bool * isMember) const;
};

} /* namespace xcc::ast */

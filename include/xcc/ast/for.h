#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/vardecl.h"

namespace xcc::ast {

class For : public Node {
public:
  std::shared_ptr<VarDecl> init;
  std::shared_ptr<Node> cond;
  std::shared_ptr<Node> step;
  std::shared_ptr<Node> body;

public:
  For(std::shared_ptr<VarDecl> init, std::shared_ptr<Node> cond, std::shared_ptr<Node> step, std::shared_ptr<Node> body);
  virtual ~For() override = default;

  static std::shared_ptr<For> create(std::shared_ptr<VarDecl> init, std::shared_ptr<Node> cond, std::shared_ptr<Node> step, std::shared_ptr<Node> body);

  llvm::Value * generateValue(codegen::ModuleContext& ctx, void * payload = nullptr) override;
};

} /* namespace xcc::ast */

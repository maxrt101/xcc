#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/fndecl.h"
#include "xcc/ast/block.h"

namespace xcc::ast {

class FnDef : public Node {
public:
  std::shared_ptr<FnDecl> decl;
  std::shared_ptr<Block> body;

public:
  FnDef(std::shared_ptr<FnDecl> decl, std::shared_ptr<Block> body);
  virtual ~FnDef() override = default;

  static std::shared_ptr<FnDef> create(std::shared_ptr<FnDecl> decl, std::shared_ptr<Block> body);

  llvm::Function * generateFunction(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

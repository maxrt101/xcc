#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/fndecl.h"
#include "xcc/ast/block.h"

namespace xcc::ast {

class FnDef : public Node {
public:
  std::shared_ptr<FnDecl> decl;
  std::shared_ptr<Block>  body;

public:
  FnDef(SourceSpan span, std::shared_ptr<FnDecl> decl, std::shared_ptr<Block> body);
  ~FnDef() override = default;

  static std::shared_ptr<FnDef> create(SourceSpan span, std::shared_ptr<FnDecl> decl, std::shared_ptr<Block> body);

  std::shared_ptr<Node> clone() override;
  void visit(std::unique_ptr<codegen::GlobalContext>& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Function * generateFunction(codegen::ModuleContext& ctx, PayloadList payload) override;

  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;

private:
  void processAttributes(codegen::ModuleContext& ctx, PayloadList payload, llvm::Function * fn);

  void generateNakedFunction(
    codegen::ModuleContext&         ctx,
    PayloadList                     payload,
    const std::shared_ptr<meta::Function>& meta_fn,
    llvm::Function *                fn,
    llvm::DISubprogram *            di_fn
  );

  void generateNormalFunction(
    codegen::ModuleContext&         ctx,
    PayloadList                     payload,
    const std::shared_ptr<meta::Function>& meta_fn,
    llvm::Function *                fn,
    llvm::DISubprogram *            di_fn
  );
};

} /* namespace xcc::ast */

#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/type.h"
#include "xcc/ast/identifier.h"

#include <string>
#include <vector>

namespace xcc::ast {

class VarDecl : public Node {
public:
  std::shared_ptr<Identifier> name;
  std::shared_ptr<Node>       type;
  std::shared_ptr<Node>       value;
  bool                        global;

public:
  VarDecl(
      SourceSpan                  span,
      std::shared_ptr<Identifier> name,
      std::shared_ptr<Node>       type,
      std::shared_ptr<Node>       value  = nullptr,
      bool                        global = false
  );

  ~VarDecl() override = default;

  static std::shared_ptr<VarDecl> create(
      SourceSpan                  span,
      std::shared_ptr<Identifier> name,
      std::shared_ptr<Node>       type,
      std::shared_ptr<Node>       value  = nullptr,
      bool                        global = false
  );

  std::shared_ptr<Node> clone() override;
  void visit(std::unique_ptr<codegen::GlobalContext>& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;

private:
  llvm::Value * generateLocal(codegen::ModuleContext& ctx, std::shared_ptr<meta::Type> meta_type, PayloadList payload);
  llvm::Value * generateGlobal(codegen::ModuleContext& ctx, std::shared_ptr<meta::Type> meta_type, PayloadList payload);
};

} /* namespace xcc::ast */

#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/identifier.h"

#include <string>
#include <vector>

namespace xcc::ast {

class ConstDecl : public Node {
public:
  std::shared_ptr<Identifier> name;
  std::shared_ptr<Node>       type;
  std::shared_ptr<Node>       value;

public:
  ConstDecl(
      SourceSpan                  span,
      std::shared_ptr<Identifier> name,
      std::shared_ptr<Node>       type,
      std::shared_ptr<Node>       value = nullptr
  );

  ~ConstDecl() override = default;

  static std::shared_ptr<ConstDecl> create(
      SourceSpan                  span,
      std::shared_ptr<Identifier> name,
      std::shared_ptr<Node>       type,
      std::shared_ptr<Node>       value = nullptr
  );

  std::shared_ptr<Node> clone() override;
  void visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Constant * generateConstant(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

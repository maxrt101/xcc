#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/type.h"
#include "xcc/ast/identifier.h"

#include <string>
#include <vector>

namespace xcc::ast {

class MemberAccess : public Node {
public:
  enum MemberAccessKind {
    MEMBER_ACCESS_VALUE = 0,
    MEMBER_ACCESS_POINTER,
  };

public:
  MemberAccessKind            kind;
  std::shared_ptr<Node>       lhs;
  std::shared_ptr<Identifier> rhs;

public:
  MemberAccess(SourceSpan span, MemberAccessKind kind, std::shared_ptr<Node> lhs, std::shared_ptr<Identifier> rhs);
  ~MemberAccess() override = default;

  static std::shared_ptr<MemberAccess> createByValue(SourceSpan span, std::shared_ptr<Node> lhs, std::shared_ptr<Identifier> rhs);
  static std::shared_ptr<MemberAccess> createByPointer(SourceSpan span, std::shared_ptr<Node> lhs, std::shared_ptr<Identifier> rhs);

  std::shared_ptr<Node> clone() override;
  void visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<xcc::meta::Type> generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

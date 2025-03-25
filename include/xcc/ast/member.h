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
  MemberAccessKind kind;
  std::shared_ptr<Node> lhs;
  std::shared_ptr<Identifier> rhs;

public:
  MemberAccess(MemberAccessKind kind, std::shared_ptr<Node> lhs, std::shared_ptr<Identifier> rhs);
  virtual ~MemberAccess() override = default;

  static std::shared_ptr<MemberAccess> createByValue(std::shared_ptr<Node> lhs, std::shared_ptr<Identifier> rhs);
  static std::shared_ptr<MemberAccess> createByPointer(std::shared_ptr<Node> lhs, std::shared_ptr<Identifier> rhs);

  llvm::Value * generateValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
  std::shared_ptr<xcc::meta::Type> generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
};

} /* namespace xcc::ast */

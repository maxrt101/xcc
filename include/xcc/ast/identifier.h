#pragma once

#include "xcc/ast/node.h"

#include <string>

namespace xcc::ast {

class Identifier : public Node {
public:
  std::string              value;
  std::vector<std::string> scope;
  NodeList                 genericArgs;

public:
  Identifier(
    SourceSpan               span,
    std::string              value,
    std::vector<std::string> scope       = {},
    NodeList                 genericArgs = {}
  );

  ~Identifier() override = default;

  static std::shared_ptr<Identifier> create(
    SourceSpan               span,
    std::string              value,
    std::vector<std::string> scope       = {},
    NodeList                 genericArgs = {}
  );

  std::shared_ptr<Node> clone() override;
  void visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  /** Returns constructed name with scope + value (e.g. `module::Enum::Value`) */
  [[nodiscard]] std::string name() const;

  /** Returns scope prefix (e.g. `module::Enum`) */
  [[nodiscard]] std::string prefix() const;

  /** Returns full name with mangled generics */
  [[nodiscard]] std::string fullName(
    codegen::ModuleContext& ctx,
    PayloadList             payload,
    bool                    isMethod    = false,
    NodeList                genericArgs = {}
  ) const;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  llvm::Value * generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) override;
  llvm::Constant * generateConstant(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) override;

private:
  /**
   * If `prefix()` (or current module prefix + `prefix()`) matches a registered enum type,
   * and `value` is a member of that enum - return constant value
   */
  llvm::Constant * checkGenerateEnum(codegen::ModuleContext& ctx, PayloadList payload);

  /**
   * If genericArgs is not empty - this identifier references a static method for generic struct
   */
  llvm::Value * generateStaticMethod(codegen::ModuleContext& ctx, PayloadList payload);
};

} /* namespace xcc::ast */

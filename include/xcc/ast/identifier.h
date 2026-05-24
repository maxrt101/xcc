#pragma once

#include "xcc/ast/node.h"

#include <string>

namespace xcc::ast {

class Type;

class Identifier : public Node {
public:
  std::string              value;
  std::vector<std::string> scope;
  NodeList                 genericArgs;

public:
  Identifier(
    SourceSpan               span,
    LexicalScope             lexicalScope,
    std::string              value,
    std::vector<std::string> scope       = {},
    NodeList                 genericArgs = {}
  );

  ~Identifier() override = default;

  static std::shared_ptr<Identifier> create(
    SourceSpan               span,
    LexicalScope             lexicalScope,
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

  /** Resolve name. If built-in type - return immediately, otherwise try to resolve qualified and unqualified names */
  std::string getResolvedName(codegen::ModuleContext& ctx) const;

  /** Generate concrete, unqualified name for generic identifier */
  std::string getConcreteName(codegen::ModuleContext& ctx, const std::string& name) const;

  /** Create a Type node from current scope */
  std::shared_ptr<Type> intoParentType();

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  llvm::Value * generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) override;
  llvm::Constant * generateConstant(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) override;

private:
  /**
   * If `scope` is an enum, and it contains a `value` element - return it's value
   */
  llvm::Constant * checkGenerateEnum(codegen::ModuleContext& ctx, PayloadList payload);

  /**
   * If genericArgs is not empty - this identifier references a static method for generic struct
   */
  std::string resolveStaticMethodName(codegen::ModuleContext& ctx, PayloadList payload);
};

} /* namespace xcc::ast */

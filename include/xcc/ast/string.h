#pragma once

#include "xcc/ast/node.h"

#include <string>

namespace xcc::ast {

class String : public Node {
private:
  struct GlobalString {
    std::string            name;
    llvm::GlobalVariable * global;
  };

public:
  std::string value;

public:
  explicit String(SourceSpan span, LexicalScope scope, std::string value);
  ~String() override = default;

  static std::shared_ptr<String> create(SourceSpan span, LexicalScope scope, std::string value);

  std::shared_ptr<Node> clone() override;
  void visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  llvm::Value * generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) override;
  llvm::Constant * generateConstant(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;

private:
  GlobalString getOrCreateGlobalString(codegen::ModuleContext& ctx);
};

} /* namespace xcc::ast */

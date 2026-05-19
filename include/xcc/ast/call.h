#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/identifier.h"
#include "xcc/ast/member.h"

#include <vector>
#include <string>

namespace xcc::ast {

class Call : public Node {
private:
  struct CalleeInfo {
    std::string                 fnName;
    bool                        isMember  = false;
    llvm::Value*                calleePtr = nullptr;
    std::shared_ptr<meta::Type> metaType  = nullptr;
  };

public:
  std::shared_ptr<Node>              callee;
  NodeList args;

public:
  Call(SourceSpan span, std::shared_ptr<Node> callee, NodeList args);
  ~Call() override = default;

  static std::shared_ptr<Call> create(SourceSpan span, std::shared_ptr<Node> callee, NodeList args);

  std::shared_ptr<Node> clone() override;
  void visit(std::unique_ptr<codegen::GlobalContext>& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;

private:
  CalleeInfo getCalleeInfo(codegen::ModuleContext& ctx, PayloadList payload, bool generateCallee);

  void getCalleeInfoForFunctionCall(codegen::ModuleContext& ctx, PayloadList payload, CalleeInfo& info, Identifier* ident, bool generateCallee);
  void getCalleeInfoForMethodCall(codegen::ModuleContext& ctx, PayloadList payload, CalleeInfo& info, MemberAccess * memberAccess);
};

} /* namespace xcc::ast */

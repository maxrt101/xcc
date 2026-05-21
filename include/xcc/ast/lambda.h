#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/block.h"
#include "xcc/ast/typed_identifier.h"

#include <string>
#include <vector>

namespace xcc::ast {

class Lambda : public Node {
public:
  NodeList                                      captures;
  std::vector<std::shared_ptr<TypedIdentifier>> args;
  std::shared_ptr<Node>                         return_type;
  std::shared_ptr<Block>                        body;
  bool                                          isVariadic;

public:
  Lambda(
      SourceSpan                                    span,
      NodeList                                      captures,
      std::vector<std::shared_ptr<TypedIdentifier>> args,
      std::shared_ptr<Node>                         return_type,
      std::shared_ptr<Block>                        body,
      bool                                          isVariadic = false
  );

  ~Lambda() override = default;

  static std::shared_ptr<Lambda> create(
      SourceSpan                                    span,
      NodeList                                      captures,
      std::vector<std::shared_ptr<TypedIdentifier>> args,
      std::shared_ptr<Node>                         return_type,
      std::shared_ptr<Block>                        body,
      bool                                          isVariadic = false
  );

  std::shared_ptr<Node> clone() override;
  void visit(std::unique_ptr<codegen::GlobalContext>& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext &ctx, PayloadList payload) override;

private:
  std::shared_ptr<meta::Type> generateLambdaType(codegen::ModuleContext &ctx, PayloadList payload);

  static uint64_t counter;
};

} /* namespace xcc::ast */

#pragma once

#include "xcc/ast/node.h"

namespace xcc::ast {

class Decomposition : public Node {
public:
  NodeList              pieces;
  std::shared_ptr<Node> value;

public:
  Decomposition(
    SourceSpan            span,
    NodeList              pieces,
    std::shared_ptr<Node> value = nullptr
  );

  ~Decomposition() override = default;

  static std::shared_ptr<Decomposition> create(
    SourceSpan            span,
    NodeList              pieces,
    std::shared_ptr<Node> value = nullptr
  );

  std::shared_ptr<Node> clone() override;
  void visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;

  std::shared_ptr<meta::Type> generateTypeForPiece(
    std::shared_ptr<meta::Type> base_type,
    size_t                      pieceIndex
  );

private:
  void decomposeListPiece(
    codegen::ModuleContext&            ctx,
    PayloadList                        payload,
    llvm::Value *                      base_val,
    std::shared_ptr<meta::Type>        base_type,
    NodeList sub_pieces
  );

  void decomposeNamedPiece(
    codegen::ModuleContext&     ctx,
    PayloadList                 payload,
    const std::string&          name,
    llvm::Value *               val,
    std::shared_ptr<meta::Type> val_type
  );
};

} /* namespace xcc::ast */

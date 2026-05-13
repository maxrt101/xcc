#pragma once

#include <llvm/IR/Instructions.h>

#include "xcc/ast/node.h"

namespace xcc::ast {

class Initializer : public Node {
public:
  struct Value {
    std::shared_ptr<Node> name, value;
  };

  std::shared_ptr<Node> value_type;
  std::vector<Value>    values;
  bool                  has_square_braces;

public:
  Initializer(SourceSpan span, std::shared_ptr<Node> value_type, std::vector<Value> values, bool has_square_braces);
  ~Initializer() override = default;

  static std::shared_ptr<Initializer> create(SourceSpan span, std::shared_ptr<Node> value_type, std::vector<Value> values, bool has_square_braces);

  std::shared_ptr<Node> clone() override;
  void visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload) override;
  llvm::Value * generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) override;
  llvm::Constant * generateConstant(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) override;

private:
  void fillStruct(codegen::ModuleContext& ctx, std::shared_ptr<meta::Type>& t, llvm::AllocaInst * alloca);
  void fillArray(codegen::ModuleContext& ctx, std::shared_ptr<meta::Type>& t, llvm::AllocaInst * alloca);
};

} /* namespace xcc::ast */

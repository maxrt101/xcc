#include "xcc/ast/block.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

Block::Block(std::vector<std::shared_ptr<Node>> body)
  : Node(AST_BLOCK), body(std::move(body)) {}

std::shared_ptr<Block> Block::create(std::vector<std::shared_ptr<Node>> body) {
  return std::make_shared<Block>(std::move(body));
}

std::shared_ptr<Node> Block::clone() {
  return withAttrs(create(cloneVector(body)));
}

void Block::visit(Visitor visitor) {
  for (auto& node : body) {
    callVisitor(node, visitor);
  }
}

std::string Block::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = "{";

  if (newline) {
    res += "\n";
  }

  for (auto& node : body) {
    if (!node->attributes.empty()) {
      res += newline ? getIndent(indent + 1) : " ";
      res += node->attributesToString(indent, newline);
    }

    res += newline ? getIndent(indent + 1) : " ";
    res += node->toString(parent, this, indent + 1, newline);

    if (!node->isAnyOf(AST_FUNCTION_DEF, AST_STRUCT, AST_MOD, AST_IF, AST_FOR, AST_WHILE)) {
      res += ";";
    }

    if (newline) {
      res += "\n";
    }
  }

  if (newline) {
    res += getIndent(indent);
  }

  res += "}";

  if (newline && (!parent || !parent->isAnyOf(AST_IF, AST_FOR, AST_WHILE))) {
    res += "\n";
  }

  return res;
}

llvm::Value * Block::generateValue(codegen::ModuleContext &ctx, PayloadList payload) {
  llvm::Value * val = nullptr;

  for (auto& node : body) {
    val = node->generateValue(ctx, {});
  }

  return val;
}

std::shared_ptr<xcc::meta::Type> Block::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  return body.back()->generateType(ctx, {});
}

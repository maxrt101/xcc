#include "xcc/ast/block.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

Block::Block(SourceSpan span, std::vector<std::shared_ptr<Node>> body)
  : Node(AST_BLOCK, span), body(std::move(body)) {}

std::shared_ptr<Block> Block::create(SourceSpan span, std::vector<std::shared_ptr<Node>> body) {
  return std::make_shared<Block>(span, std::move(body));
}

std::shared_ptr<Node> Block::clone() {
  return withAttrs(create(span, cloneVector(body)));
}

void Block::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  for (auto& node : body) {
    callVisitor(node, visitor, ignoreSubtree);
  }
}

std::string Block::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(indent, newline) +  "{";

  if (newline) {
    res += "\n";
  }

  for (auto& node : body) {
    res += newline ? getIndent(indent + 1) : " ";
    res += node->toString(parent, this, indent + 1, newline);

    if (!node->isAnyOf(AST_FUNCTION_DEF, AST_STRUCT, AST_MOD, AST_IF, AST_FOR, AST_WHILE, AST_MACRO, AST_BLOCK)) {
      res += ";";
    }

    if (newline) {
      res += "\n";
    }
  }

  res += newline ? getIndent(indent) : " ";

  res += "}";

  if (newline && (!parent || !parent->isAnyOf(AST_IF, AST_FOR, AST_WHILE))) {
    res += "\n";
  }

  return res;
}

llvm::Value * Block::generateValue(codegen::ModuleContext &ctx, PayloadList payload) {
  ctx.pushScope();

  llvm::Value * val = nullptr;

  for (auto& node : body) {
    val = node->generateValue(ctx, {});
  }

  ctx.popScope();

  return val;
}

std::shared_ptr<xcc::meta::Type> Block::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  codegen::ModuleContext::ScopedPhantomList variables;

  for (auto& node : body) {
    if (node->is(AST_VAR_DECL)) {
      auto vardecl = node->as<VarDecl>();
      auto name = vardecl->name->name();

      variables[name] = vardecl->type->generateType(ctx, payload);
    }
  }

  auto phantoms = ctx.phantomScope(variables);

  return body.back()->generateType(ctx, {});;
}

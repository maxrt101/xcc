#include "xcc/ast/block.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

Block::Payload::Payload(std::shared_ptr<meta::Type> type)
  : Node::Payload(AST_BLOCK), type(std::move(type)) {}

std::shared_ptr<Node::Payload> Block::Payload::create(std::shared_ptr<meta::Type> type) {
  return std::dynamic_pointer_cast<Node::Payload>(
      std::make_shared<Block::Payload>(std::move(type))
  );
}

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
  ctx.pushScope(span);
  ctx.setDebugLocation(span);

  llvm::Value * val = nullptr;

  for (size_t i = 0; i < body.size() - 1; ++i) {
    body[i]->generateValue(ctx, payload);
  }

  if (auto p = selectPayloadFirst(payload)) {
    // If type hint was passed for AST_BLOCK, repackage it for AST_INIT
    auto t = p->as<Payload>()->type;
    payload = extendPayload(excludePayload(payload, AST_BLOCK), Initializer::Payload::create(t));
  }

  val = body.back()->generateValue(ctx, payload);

  ctx.popScope();

  return val;
}

std::shared_ptr<xcc::meta::Type> Block::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  auto phantoms = ctx.phantomScope({});

  for (auto& node : body) {
    if (node->is(AST_VAR_DECL)) {
      auto vardecl = node->as<VarDecl>();

      phantoms.add(vardecl->name->name(), vardecl->type->generateType(ctx, payload));
    }
  }

  if (auto p = selectPayloadFirst(payload)) {
    // If type hint was passed for AST_BLOCK, repackage it for AST_INIT
    auto t = p->as<Payload>()->type;
    payload = extendPayload(excludePayload(payload, AST_BLOCK), Initializer::Payload::create(t));
  }

  return body.back()->generateType(ctx, payload);
}

#include "xcc/ast/block.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

template <typename T>
static T * generate(xcc::codegen::ModuleContext &ctx, Node::PayloadList payload, Block& block) {
  if (block.body.empty()) return nullptr;

  ctx.pushScope(block.span);
  ctx.setDebugLocation(block.span);

  T * val = nullptr;

  for (size_t i = 0; i < block.body.size() - 1; ++i) {
    if constexpr (std::is_same_v<T, llvm::Value>) {
      block.body[i]->generateValue(ctx, payload);
    } else {
      block.body[i]->generateConstant(ctx, payload);
    }
  }

  if (auto p = block.selectPayloadFirst(payload)) {
    // If type hint was passed for AST_BLOCK, repackage it for AST_INIT
    auto t = p->as<Block::Payload>()->type;
    payload = Node::extendPayload(Node::excludePayload(payload, AST_BLOCK), Initializer::Payload::create(t));
  }

  if constexpr (std::is_same_v<T, llvm::Value>) {
    val = block.body.back()->generateValue(ctx, payload);
  } else {
    val = block.body.back()->generateConstant(ctx, payload);
  }

  ctx.popScope();

  return val;
}

Block::Payload::Payload(std::shared_ptr<meta::Type> type)
  : Node::Payload(AST_BLOCK), type(std::move(type)) {}

std::shared_ptr<Node::Payload> Block::Payload::create(std::shared_ptr<meta::Type> type) {
  return std::dynamic_pointer_cast<Node::Payload>(
      std::make_shared<Block::Payload>(std::move(type))
  );
}

Block::Block(SourceSpan span, LexicalScope scope, NodeList body)
  : Node(AST_BLOCK, span, scope), body(std::move(body)) {}

std::shared_ptr<Block> Block::create(SourceSpan span, LexicalScope scope, NodeList body) {
  return std::make_shared<Block>(span, scope, std::move(body));
}

std::shared_ptr<Node> Block::clone() {
  return withAttrs(create(span, scope, cloneVector(body)));
}

void Block::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  for (auto& node : body) {
    callVisitor(globalContext, node, visitor, ignoreSubtree);
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

llvm::Constant * Block::generateConstant(codegen::ModuleContext& ctx, PayloadList payload) {
  return generate<llvm::Constant>(ctx, payload, *this);
}

llvm::Value * Block::generateValue(codegen::ModuleContext &ctx, PayloadList payload) {
  return generate<llvm::Value>(ctx, payload, *this);
}

std::shared_ptr<xcc::meta::Type> Block::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  if (body.empty()) return meta::Type::createVoid();

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

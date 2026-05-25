#include "xcc/ast/block.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

template <typename T>
static T * generate(xcc::codegen::ModuleContext &ctx, Node::PayloadList payload, Block& block, bool is_fn_block) {
  if (block.body.empty()) return nullptr;

  if (!is_fn_block) {
    ctx.pushScope(block.span);
  }

  ctx.setDebugLocation(block.span);

  // Workaround: stop propagating is_fn_block, without excluding payload
  // Cannot exclude because ast::Init may depend on it
  if (is_fn_block) {
    for (auto& p : Node::selectPayloadFor(payload, AST_BLOCK)) {
      p->as<Block::Payload>()->is_fn_block = false;
    }
  }

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
    payload = Node::extendPayload(payload, Initializer::Payload::create(t));
  }

  if constexpr (std::is_same_v<T, llvm::Value>) {
    val = block.body.back()->generateValue(ctx, payload);
  } else {
    val = block.body.back()->generateConstant(ctx, payload);
  }

  if (!is_fn_block) {
    ctx.popScope();
  }

  return val;
}

Block::Payload::Payload(std::shared_ptr<meta::Type> type, bool is_fn_block)
  : Node::Payload(AST_BLOCK), type(std::move(type)), is_fn_block(is_fn_block) {}

std::shared_ptr<Node::Payload> Block::Payload::create(std::shared_ptr<meta::Type> type, bool is_fn_block) {
  return std::dynamic_pointer_cast<Node::Payload>(
      std::make_shared<Block::Payload>(std::move(type), is_fn_block)
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

    if (!node->isAnyOf(AST_FUNCTION_DEF, AST_STRUCT, AST_MOD, AST_IF, AST_FOR, AST_WHILE, AST_MACRO, AST_BLOCK, AST_LAMBDA)) {
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
  bool is_fn_block = false;

  if (auto p = selectPayloadFirst(payload)) {
    is_fn_block = p->as<Payload>()->is_fn_block;
  }

  return generate<llvm::Constant>(ctx, payload, *this, is_fn_block);
}

llvm::Value * Block::generateValue(codegen::ModuleContext &ctx, PayloadList payload) {
  bool is_fn_block = false;

  if (auto p = selectPayloadFirst(payload)) {
    is_fn_block = p->as<Payload>()->is_fn_block;
  }

  return generate<llvm::Value>(ctx, payload, *this, is_fn_block);
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

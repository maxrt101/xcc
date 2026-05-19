#include "xcc/ast.h"
#include "xcc/util/log.h"
#include "xcc/codegen.h"

using namespace xcc;
using namespace xcc::ast;

static auto logger = xcc::util::log::Logger("AST");

bool ast::isOrIsLastInBlock(std::shared_ptr<Node> node, NodeType type) {
  if (node->is(type)) {
    return true;
  }

  if (node->is(AST_BLOCK)) {
    auto block = node->as<Block>();
    return isOrIsLastInBlock(block->body.back(), type);
  }

  return false;
}

std::shared_ptr<Node> ast::getOrGetLastInBlock(std::shared_ptr<Node> node, NodeType type) {
  if (node->is(type)) {
    return node;
  }

  if (node->is(AST_BLOCK)) {
    auto block = node->as<Block>();
    return getOrGetLastInBlock(block->body.back(), type);
  }

  return nullptr;
}

std::shared_ptr<Node> ast::getOrGetLastInBlock(std::shared_ptr<Node> node) {
  if (node->is(AST_BLOCK)) {
    auto block = node->as<Block>();
    return getOrGetLastInBlock(block->body.back());
  }

  return node;
}

void subtree::replaceIdentifierWithNode(const std::shared_ptr<Node>& node, const std::string& oldValue, std::shared_ptr<Node> newNode) {
  std::unique_ptr<codegen::GlobalContext> ctx = {nullptr};

  node->visit(ctx, [&](auto node) -> std::shared_ptr<Node> {
    if (node->is(AST_EXPR_IDENTIFIER) && node->template as<Identifier>()->name() == oldValue) {
      return newNode;
    }

    return nullptr;
  }, {});
}

void subtree::replaceIdentifier(const std::shared_ptr<Node>& node, const std::string& oldValue, const std::string& newValue) {
  std::unique_ptr<codegen::GlobalContext> ctx = {nullptr};

  node->visit(ctx, [&](auto node) -> std::shared_ptr<Node> {
    if (node->is(AST_EXPR_IDENTIFIER) && node->template as<Identifier>()->name() == oldValue) {
      return Identifier::create(node->span, newValue);
    }

    return nullptr;
  }, {});
}

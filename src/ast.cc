#include "xcc/ast.h"
#include "xcc/util/log.h"

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

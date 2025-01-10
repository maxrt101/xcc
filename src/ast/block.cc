#include "xcc/ast/block.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

Block::Block(std::vector<std::shared_ptr<Node>> body)
  : Node(AST_BLOCK), body(std::move(body)) {}

std::shared_ptr<Block> Block::create(std::vector<std::shared_ptr<Node>> body) {
  return std::make_shared<Block>(body);
}

llvm::Value * Block::generateValue(codegen::ModuleContext &ctx, void * payload) {
  llvm::Value * val = nullptr;

  for (auto& node : body) {
    val = node->generateValue(ctx);
  }

  return val;
}

std::shared_ptr<xcc::meta::Type> Block::generateType(codegen::ModuleContext& ctx, void * payload) {
  return body.back()->generateType(ctx);
}

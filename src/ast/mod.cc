#include "xcc/ast/mod.h"

using namespace xcc::ast;

Module::Module(std::shared_ptr<Node> name, std::shared_ptr<Block> body) : Node(AST_MOD), name(std::move(name)), body(std::move(body)) {}

std::shared_ptr<Module> Module::create(std::shared_ptr<Node> name, std::shared_ptr<Block> body) {
  return std::make_shared<Module>(std::move(name), std::move(body));
}


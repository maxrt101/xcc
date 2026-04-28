#include "xcc/ast/mod.h"
#include "xcc/exceptions.h"
#include "xcc/ast/identifier.h"

using namespace xcc::ast;

Module::Module(std::shared_ptr<Node> name, std::shared_ptr<Block> body) : Node(AST_MOD), name(std::move(name)), body(std::move(body)) {}

std::shared_ptr<Module> Module::create(std::shared_ptr<Node> name, std::shared_ptr<Block> body) {
  return std::make_shared<Module>(std::move(name), std::move(body));
}

std::string Module::getName() const {
  assertThrow(name->is(AST_EXPR_IDENTIFIER), CodegenException("Module name must be an identifier"));

  return name->as<Identifier>()->name();
}


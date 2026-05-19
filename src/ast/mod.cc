#include "xcc/ast/mod.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/ast/identifier.h"
#include <format>

using namespace xcc::ast;

Module::Module(SourceSpan span, std::shared_ptr<Node> name, std::shared_ptr<Block> body)
  : Node(AST_MOD, span), name(std::move(name)), body(std::move(body)) {}

std::shared_ptr<Module> Module::create(SourceSpan span, std::shared_ptr<Node> name, std::shared_ptr<Block> body) {
  return std::make_shared<Module>(span, std::move(name), std::move(body));
}

std::shared_ptr<Node> Module::clone() {
  return withAttrs(create(span, name->clone(), body ? cast<Block>(body->clone()) : nullptr));
}

void Module::visit(std::unique_ptr<codegen::GlobalContext>& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  globalContext->pushModule(getName());
  callVisitor(globalContext, name, visitor, ignoreSubtree);
  callVisitor(globalContext, body, visitor, ignoreSubtree);
  globalContext->popModule();
}

std::string Module::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(indent, newline) + std::format("module {} {}",
    name->toString(parent, this, indent, false),
    body->toString(parent, this, indent, newline)
  );
}

std::string Module::getName() const {
  assertRaiseFromNode(name->is(AST_EXPR_IDENTIFIER), Error(ERROR_MOD_NAME_NOT_IDENTIFIER, name->span, "Module name must be an identifier"), this);

  return name->as<Identifier>()->name();
}


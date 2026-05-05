#include "xcc/ast/mod.h"
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

void Module::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(name, visitor, ignoreSubtree);
  callVisitor(body, visitor, ignoreSubtree);
}

std::string Module::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(indent, newline) + std::format("module {} {}",
    name->toString(parent, this, indent, false),
    body->toString(parent, this, indent, newline)
  );
}

std::string Module::getName() const {
  assertRaise(name->is(AST_EXPR_IDENTIFIER), Error(ERROR_MOD_NAME_NOT_IDENTIFIER, name->span, "Module name must be an identifier"));

  return name->as<Identifier>()->name();
}


#include "xcc/ast/mod.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/ast/identifier.h"
#include <format>

using namespace xcc::ast;

Module::Module(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> name, std::shared_ptr<Block> body)
  : Node(AST_MOD, span, scope), name(std::move(name)), body(std::move(body)) {}

std::shared_ptr<Module> Module::create(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> name, std::shared_ptr<Block> body) {
  return std::make_shared<Module>(span, scope, std::move(name), std::move(body));
}

std::shared_ptr<Node> Module::clone() {
  return withAttrs(create(span, scope, name->clone(), body ? cast<Block>(body->clone()) : nullptr));
}

void Module::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  globalContext.pushModule(getName(), getPath());
  callVisitor(globalContext, name, visitor, ignoreSubtree);
  callVisitor(globalContext, body, visitor, ignoreSubtree);
  globalContext.popModule();
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

std::string Module::getPath() const {
  if (hasAttribute("__xcc_tag_used_from")) {
    return getAttribute("__xcc_tag_used_from").args[0]->as<String>()->value;
  }

  return "";
}

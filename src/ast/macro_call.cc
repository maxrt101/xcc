#include "xcc/ast/macro_call.h"
#include <format>

using namespace xcc;
using namespace xcc::ast;

MacroCall::MacroCall(
    SourceSpan                  span,
    LexicalScope                scope,
    std::shared_ptr<Identifier> name,
    NodeList                    args
) : Node(AST_EXPR_MACRO_CALL, span, scope), name(std::move(name)), args(std::move(args)) {}

std::shared_ptr<MacroCall> MacroCall::create(
  SourceSpan                  span,
  LexicalScope                scope,
  std::shared_ptr<Identifier> name,
  NodeList                    args
) {
  return std::make_shared<MacroCall>(span, scope, std::move(name), std::move(args));
}

std::shared_ptr<Node> MacroCall::clone() {
  return withAttrs(create(
    span, scope,
    cast<Identifier>(name->clone()),
    cloneVector(args)
  ));
}

void MacroCall::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  for (auto& arg : args) {
    callVisitor(globalContext, arg, visitor, ignoreSubtree);
  }

  callVisitor(globalContext, name, visitor, ignoreSubtree);
}

std::string MacroCall::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(0, false) + std::format("{}!(", name->toString(parent, this, indent, false));

  for (size_t i = 0; i < args.size(); ++i) {
    res += args[i]->toString(parent, this, indent, false);

    if (i + 1 < args.size()) {
      res += ", ";
    }
  }

  return res + ")";
}

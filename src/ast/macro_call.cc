#include "xcc/ast/macro_call.h"
#include <format>

using namespace xcc;
using namespace xcc::ast;

MacroCall::MacroCall(
    SourceSpan                         span,
    std::shared_ptr<Identifier>        name,
    std::vector<std::shared_ptr<Node>> args
) : Node(AST_EXPR_MACRO_CALL, span), name(std::move(name)), args(std::move(args)) {}

std::shared_ptr<MacroCall> MacroCall::create(
  SourceSpan                         span,
  std::shared_ptr<Identifier>        name,
  std::vector<std::shared_ptr<Node>> args
) {
  return std::make_shared<MacroCall>(span, std::move(name), std::move(args));
}

std::shared_ptr<Node> MacroCall::clone() {
  return withAttrs(create(
    span,
    cast<Identifier>(name->clone()),
    cloneVector(args)
  ));
}

void MacroCall::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  for (auto& arg : args) {
    callVisitor(arg, visitor, ignoreSubtree);
  }

  callVisitor(name, visitor, ignoreSubtree);
}

std::string MacroCall::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = std::format("{}!(", name->toString(parent, this, indent, false));

  for (size_t i = 0; i < args.size(); ++i) {
    res += args[i]->toString(parent, this, indent, false);

    if (i + 1 < args.size()) {
      res += ", ";
    }
  }

  return res + ")";
}

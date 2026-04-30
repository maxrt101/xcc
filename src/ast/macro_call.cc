#include "xcc/ast/macro_call.h"
#include <format>

using namespace xcc;
using namespace xcc::ast;

MacroCall::MacroCall(
    std::shared_ptr<Identifier>        name,
    std::vector<std::shared_ptr<Node>> args
) : Node(AST_EXPR_MACRO_CALL), name(std::move(name)), args(std::move(args)) {}

std::shared_ptr<MacroCall> MacroCall::create(
  std::shared_ptr<Identifier>        name,
  std::vector<std::shared_ptr<Node>> args
) {
  return std::make_shared<MacroCall>(std::move(name), std::move(args));
}

std::shared_ptr<Node> MacroCall::clone() {
  return withAttrs(create(
    cast<Identifier>(name->clone()),
    cloneVector(args)
  ));
}

void MacroCall::visit(Visitor visitor) {
  callVisitor(name, visitor);

  for (auto& arg : args) {
    callVisitor(arg, visitor);
  }
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

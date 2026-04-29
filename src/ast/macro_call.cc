#include "xcc/ast/macro_call.h"
#include <utility>

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
  // TODO: Decide if needs to be visited
}


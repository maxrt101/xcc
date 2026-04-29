#include "xcc/ast/macro.h"
#include <utility>

using namespace xcc;
using namespace xcc::ast;

Macro::Macro(
    std::shared_ptr<Identifier>              name,
    std::vector<std::shared_ptr<Identifier>> args,
    std::shared_ptr<Block>                   body
) : Node(AST_MACRO), name(std::move(name)), args(std::move(args)), body(std::move(body)) {}

std::shared_ptr<Macro> Macro::create(
  std::shared_ptr<Identifier>              name,
  std::vector<std::shared_ptr<Identifier>> args,
  std::shared_ptr<Block>                   body
) {
  return std::make_shared<Macro>(std::move(name), std::move(args), std::move(body));
}

std::shared_ptr<Node> Macro::clone() {
  return withAttrs(create(
    cast<Identifier>(name->clone()),
    cloneVector(args),
    cast<Block>(body->clone())
  ));
}

void Macro::visit(Visitor visitor) {
  // TODO: Decide if needs to be visited
}


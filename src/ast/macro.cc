#include "xcc/ast/macro.h"
#include <format>

using namespace xcc;
using namespace xcc::ast;

Macro::Macro(
    SourceSpan                               span,
    std::shared_ptr<Identifier>              name,
    std::vector<std::shared_ptr<Identifier>> args,
    std::shared_ptr<Block>                   body
) : Node(AST_MACRO, span), name(std::move(name)), args(std::move(args)), body(std::move(body)), native(false) {}

Macro::Macro(
    std::shared_ptr<Identifier>              name,
    std::vector<std::shared_ptr<Identifier>> args,
    NativeFn                                 fn
) : Node(AST_MACRO, {}), name(std::move(name)), args(std::move(args)), native(true), fn(fn) {}

std::shared_ptr<Macro> Macro::create(
  SourceSpan                               span,
  std::shared_ptr<Identifier>              name,
  std::vector<std::shared_ptr<Identifier>> args,
  std::shared_ptr<Block>                   body
) {
  return std::make_shared<Macro>(span, std::move(name), std::move(args), std::move(body));
}

std::shared_ptr<Macro> Macro::createNative(
    std::shared_ptr<Identifier>              name,
    std::vector<std::shared_ptr<Identifier>> args,
    NativeFn                                 fn
) {
  return std::make_shared<Macro>(std::move(name), std::move(args), fn);
}

std::shared_ptr<Node> Macro::clone() {
  return withAttrs(create(
    span,
    cast<Identifier>(name->clone()),
    cloneVector(args),
    cast<Block>(body->clone())
  ));
}

void Macro::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(name, visitor, ignoreSubtree);

  for (auto& arg : args) {
    callVisitor(arg, visitor, ignoreSubtree);
  }

  callVisitor(body, visitor, ignoreSubtree);
}

std::string Macro::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = std::format("macro {}(", name->toString(parent, this, indent, newline));

  for (size_t i = 0; i < args.size(); ++i) {
    res += args[i]->toString(parent, this, indent, false);

    if (i + 1 < args.size()) {
      res += ", ";
    }
  }

  res += std::format(") {}", body->toString(parent, this, indent, newline));

  return res;
}


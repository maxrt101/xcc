#include "xcc/ast/macro.h"
#include <format>

using namespace xcc;
using namespace xcc::ast;

Macro::Macro(
    SourceSpan                               span,
    LexicalScope                             scope,
    std::shared_ptr<Identifier>              name,
    std::vector<std::shared_ptr<Identifier>> args,
    std::shared_ptr<Block>                   body
) : Node(AST_MACRO, span, scope), name(std::move(name)), args(std::move(args)), body(std::move(body)), native(false), variadic(false) {}

Macro::Macro(
    LexicalScope                             scope,
    std::shared_ptr<Identifier>              name,
    std::vector<std::shared_ptr<Identifier>> args,
    NativeFn                                 fn,
    bool                                     variadic
) : Node(AST_MACRO, SourceSpan::builtin(), scope), name(std::move(name)), args(std::move(args)), native(true), fn(fn), variadic(variadic) {}

std::shared_ptr<Macro> Macro::create(
  SourceSpan                               span,
  LexicalScope                             scope,
  std::shared_ptr<Identifier>              name,
  std::vector<std::shared_ptr<Identifier>> args,
  std::shared_ptr<Block>                   body
) {
  return std::make_shared<Macro>(span, scope, std::move(name), std::move(args), std::move(body));
}

std::shared_ptr<Macro> Macro::createNative(
    LexicalScope                             scope,
    std::shared_ptr<Identifier>              name,
    std::vector<std::shared_ptr<Identifier>> args,
    NativeFn                                 fn,
    bool                                     variadic
) {
  return std::make_shared<Macro>(scope, std::move(name), std::move(args), fn, variadic);
}

std::shared_ptr<Node> Macro::clone() {
  return withAttrs(create(
    span, scope,
    cast<Identifier>(name->clone()),
    cloneVector(args),
    cast<Block>(body->clone())
  ));
}

void Macro::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(globalContext, name, visitor, ignoreSubtree);

  for (auto& arg : args) {
    callVisitor(globalContext, arg, visitor, ignoreSubtree);
  }

  callVisitor(globalContext, body, visitor, ignoreSubtree);
}

std::string Macro::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(indent, newline) + std::format("macro {}(", name->toString(parent, this, indent, newline));

  for (size_t i = 0; i < args.size(); ++i) {
    res += args[i]->toString(parent, this, indent, false);

    if (i + 1 < args.size()) {
      res += ", ";
    }
  }

  if (variadic) {
    res += "...";
  }

  res += std::format(") {}", body->toString(parent, this, indent, newline));

  return res;
}


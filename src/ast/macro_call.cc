#include "xcc/ast/macro_call.h"
#include "xcc/exceptions.h"
#include "xcc/codegen.h"
#include "xcc/xcc.h"

using namespace xcc;
using namespace xcc::ast;

// TODO: Make a member of MacroCall
static void processMacroCall(
  codegen::GlobalContext&           globalContext,
  const std::shared_ptr<Macro>&     macro,
  const std::shared_ptr<MacroCall>& call,
  const std::shared_ptr<Node>&      body
) {
  body->visit(globalContext, [macro, call](auto node) -> std::shared_ptr<Node> {
    if (node->is(AST_EXPR_IDENTIFIER)) {
      auto arg = node->template as<Identifier>()->name();

      int argn = -1;

      for (int i = 0; i < macro->args.size(); ++i) {
        if (macro->args[i]->value == arg) {
          argn = i;
          break;
        }
      }

      if (argn == -1) {
        return nullptr;
      }

      return call->args[argn]->clone();
    }

    return nullptr;
  }, {});
}

// TODO: Make a member of MacroCall
static void markExpandedMacro(
  std::shared_ptr<Node> body,
  Node::Attribute       attr,
  SourceSpan            span
) {
  body->addAttribute(attr);
  body->span = span;

  std::unique_ptr<codegen::GlobalContext> ctx = {nullptr};

  body->visit(*ctx, [&attr, &span](auto node) -> std::shared_ptr<Node> {
    if (!node->hasAttribute(attr.name)) {
      node->addAttribute(attr);
      node->span = span;
    }

    return nullptr;
  }, {});
}


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

llvm::Value * MacroCall::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  return expand(ctx, payload)->generateValue(ctx, payload);
}

std::shared_ptr<meta::Type> MacroCall::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  return expand(ctx, payload)->generateType(ctx, payload);
}

std::shared_ptr<Node> MacroCall::expand(codegen::ModuleContext& ctx, PayloadList payload) {
  auto name  = this->name->getResolvedName(ctx);
  auto macro = ctx.globalContext.getMacro(name);
  auto call = cast<MacroCall>(clone());

  assertRaise(macro != nullptr, Error(ERROR_UNKNOWN_MACRO, this->name->span, "'{}'", name));

  for (auto& arg : call->args) {
    arg->visit(ctx.globalContext, [&ctx, payload](auto node) -> std::shared_ptr<Node> {
      if (node->is(AST_EXPR_MACRO_CALL)) {
        return node->template as<MacroCall>()->expand(ctx, payload);
      }
      return nullptr;
    }, {});

    while (arg->is(AST_EXPR_MACRO_CALL)) {
      arg = arg->template as<MacroCall>()->expand(ctx, payload);
    }
  }

  if (macro->variadic) {
    assertRaise(macro->args.size() <= this->args.size(), Error(ERROR_MACRO_CALL_ARG_COUNT_MISMATCH, this->span, "'{}'", name));
  } else {
    assertRaise(macro->args.size() == this->args.size(), Error(ERROR_MACRO_CALL_ARG_COUNT_MISMATCH, this->span, "'{}'", name));
  }

  if (macro->native) {
    auto res = macro->fn(ctx, *call);

    // Attach expansion markers & set span to this site
    markExpandedMacro(res, {
      "__xcc_macro_expanded_from",
      {Identifier::create(macro->span, macro->scope, name)},
      this->span
    }, this->span);

    return res;
  }

  auto body = cast<Block>(macro->body->clone());

  try {
    processMacroCall(ctx.globalContext, macro, call, body);
    // registerCustomTypes(globalContext, body);

    // processMacros(globalContext, body);
  } catch (CompilationException& ex) {
    ex.error.note(macro->span, "During expansion of macro {}", name).raise();
  }

  // Attach expansion markers & set span to call site
  markExpandedMacro(body, {
    "__xcc_macro_expanded_from",
    {Identifier::create(macro->span, macro->scope, name)},
    this->span
  }, this->span);

  bool isInAnotherFile = (this->span.fileId != ctx.globalContext.globalModule->file);

  if (isInAnotherFile && body->is(AST_BLOCK)) {
    body = moduleReplaceDefinitions(body->scope, body);
  }

  return body;
}



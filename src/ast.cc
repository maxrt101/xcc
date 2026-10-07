#include "xcc/ast.h"
#include "xcc/util/log.h"
#include "xcc/codegen.h"

using namespace xcc;
using namespace xcc::ast;

static auto& logger = xcc::log::Logger::get("AST");

Monomorphizer::Monomorphizer(
  codegen::GlobalContext& ctx,
  const std::string&      baseName,
  const std::string&      genericName,
  const std::string&      concreteName,
  const std::string&      concreteUnqualifiedName,
  const NodeList&         params,
  const NodeList&         args
) : globalContext(ctx), baseName(baseName), genericName(genericName), concreteName(concreteName), concreteUnqualifiedName(concreteUnqualifiedName) {
  for (size_t i = 0; i < params.size(); ++i) {
    assertRaiseFromNode(params[i]->is(AST_EXPR_IDENTIFIER), Error(ERROR_INTERNAL_UNEXPECTED_NODE, params[i]->span,
      "Monomorphizer expected an Identifier as generic parameter name"), params[i].get());

    auto param = params[i]->as<Identifier>()->value;

    substitutions[param] = args[i];
  }
}

// TODO: Document this shit
void Monomorphizer::apply(const std::shared_ptr<Node>& node) {
  node->visit(this->globalContext, [this](const auto& n) -> std::shared_ptr<Node> {
    for (auto& s : n->scope) {
      if (s == baseName) {
        s = concreteUnqualifiedName;
      } else if (s == genericName) {
        s = concreteName;
      }
    }

    if (n->is(AST_EXPR_IDENTIFIER)) {
      auto id = n->template as<Identifier>();

      if (id->genericArgs.empty() && id->scope.empty() && id->value == genericName) {
        id->value = concreteName;
      }

      if (id->genericArgs.empty() && id->scope.empty() && id->value == baseName) {
        id->value = concreteUnqualifiedName;
      }

      if (!id->scope.empty() && substitutions.contains(id->scope[0])) {
        if (!id->scope.empty() && substitutions.contains(id->scope[0])) {
          auto sub_node = substitutions[id->scope[0]];
          Identifier * param = nullptr;

          while (sub_node->is(AST_EXPR_TYPE)) {
            sub_node = sub_node->template as<Type>()->name;
          }

          if (sub_node->is(AST_EXPR_IDENTIFIER)) {
            param = sub_node->template as<Identifier>();
          }

          if (param) {
            LexicalScope new_scope = param->scope;
            new_scope.push_back(param->value);

            for (size_t i = 1; i < id->scope.size(); ++i) {
              new_scope.push_back(id->scope[i]);
            }

            id->scope = new_scope;
          }
        }
      }

      // Specifically for macro expansions, as a type in macro call context is treated as an identifier
      if (substitutions.contains(id->value)) {
        return substitutions[id->value]->clone();
      }
    }

    if (n->is(AST_EXPR_TYPE)) {
      auto type = n->template as<Type>();

      if (type->name && type->name->is(AST_EXPR_IDENTIFIER)) {
        auto id = type->name->template as<Identifier>()->value;
        if (substitutions.contains(id)) {
          return substitutions[id]->clone();
        }
      }
    }

    return nullptr;
  }, {});
}

bool ast::isOrIsLastInBlock(std::shared_ptr<Node> node, NodeType type) {
  if (node->is(type)) {
    return true;
  }

  if (node->is(AST_BLOCK)) {
    auto block = node->as<Block>();
    return isOrIsLastInBlock(block->body.back(), type);
  }

  return false;
}

std::shared_ptr<Node> ast::getOrGetLastInBlock(std::shared_ptr<Node> node, NodeType type) {
  if (node->is(type)) {
    return node;
  }

  if (node->is(AST_BLOCK)) {
    auto block = node->as<Block>();
    return getOrGetLastInBlock(block->body.back(), type);
  }

  return nullptr;
}

std::shared_ptr<Node> ast::getOrGetLastInBlock(std::shared_ptr<Node> node) {
  if (node->is(AST_BLOCK)) {
    auto block = node->as<Block>();
    return getOrGetLastInBlock(block->body.back());
  }

  return node;
}

std::shared_ptr<Node> ast::expand(std::shared_ptr<Node> node, codegen::ModuleContext& ctx) {
  if (!node) return node;

  if (node->is(AST_EXPR_MACRO_CALL)) {
    return expand(node->as<MacroCall>()->expand(ctx, {}), ctx);
  }

  return node;
}

std::shared_ptr<Node> ast::expandConstantExpressionNode(std::shared_ptr<Node> node, codegen::ModuleContext& ctx) {
  if (!node) return node;

  node = expand(node, ctx);

  if (node->is(AST_EXPR_BINARY)) {
    auto bin = node->as<Binary>();

    auto lhs = expand(bin->lhs, ctx);
    auto rhs = expand(bin->rhs, ctx);

    if (lhs->is(AST_EXPR_STRING) && rhs->is(AST_EXPR_STRING)) {
      auto l_str = lhs->as<String>()->value;
      auto r_str = rhs->as<String>()->value;
      bool result = false;

      switch (bin->operation.type) {
        case TOKEN_EQUALS_EQUALS:  result = (l_str == r_str); break;
        case TOKEN_NOT_EQUALS:     result = (l_str != r_str); break;
        case TOKEN_GREATER:        result = (l_str >  r_str); break;
        case TOKEN_GREATER_EQUALS: result = (l_str >= r_str); break;
        case TOKEN_LESS:           result = (l_str <  r_str); break;
        case TOKEN_LESS_EQUALS:    result = (l_str <= r_str); break;
        default:
          Error(ERROR_UNIMPLEMENTED, bin->operation.span,
            "Compile-time string operation '{}' is not supported", bin->operation.toString())
            .raiseFromNode(bin);
      }

      return Node::cast(Number::createInteger(bin->span, bin->scope, result ? 1 : 0));
    }

    if (lhs->is(AST_EXPR_NUMBER) && rhs->is(AST_EXPR_NUMBER)) {
      auto l_num = lhs->as<Number>();
      auto r_num = rhs->as<Number>();

      assertRaiseFromNode(l_num->tag == Number::INTEGER && r_num->tag == Number::INTEGER,
        Error(ERROR_UNIMPLEMENTED, bin->span, "Only integer arithmetic is supported at compile-time"), bin);

      int64_t l = l_num->value.integer;
      int64_t r = r_num->value.integer;
      int64_t res = 0;

      switch (bin->operation.type) {
        case TOKEN_PLUS:           res = l + r; break;
        case TOKEN_MINUS:          res = l - r; break;
        case TOKEN_STAR:           res = l * r; break;
        case TOKEN_SLASH:
          assertRaiseFromNode(r != 0, Error(ERROR_UNIMPLEMENTED, bin->span, "Compile-time division by zero"), bin);
          res = l / r;
          break;
        case TOKEN_EQUALS_EQUALS:  res = (l == r); break;
        case TOKEN_NOT_EQUALS:     res = (l != r); break;
        case TOKEN_GREATER:        res = (l >  r); break;
        case TOKEN_GREATER_EQUALS: res = (l >= r); break;
        case TOKEN_LESS:           res = (l <  r); break;
        case TOKEN_LESS_EQUALS:    res = (l <= r); break;
        case TOKEN_AND:            res = (l && r); break;
        case TOKEN_OR:             res = (l || r); break;
        default:
          Error(ERROR_UNIMPLEMENTED, bin->operation.span,
            "Compile-time operation '{}' is not supported", bin->operation.toString())
            .raiseFromNode(bin);
      }

      return Node::cast(Number::createInteger(bin->span, bin->scope, res));
    }

    Error(ERROR_ATTR_ARG_TYPE_MISMATCH, bin->span,
      "Compile-time binary operations must be between two strings or two integers")
      .raiseFromNode(bin);
  }

  return Node::cast(node);
}

void subtree::replaceIdentifierWithNode(const std::shared_ptr<Node>& node, const std::string& oldValue, std::shared_ptr<Node> newNode) {
  std::unique_ptr<codegen::GlobalContext> ctx = {nullptr};

  node->visit(*ctx, [&](auto node) -> std::shared_ptr<Node> {
    if (node->is(AST_EXPR_IDENTIFIER) && node->template as<Identifier>()->name() == oldValue) {
      return newNode;
    }

    return nullptr;
  }, {});
}

void subtree::replaceIdentifier(const std::shared_ptr<Node>& node, const std::string& oldValue, const std::string& newValue) {
  std::unique_ptr<codegen::GlobalContext> ctx = {nullptr};

  node->visit(*ctx, [&](auto node) -> std::shared_ptr<Node> {
    if (node->is(AST_EXPR_IDENTIFIER) && node->template as<Identifier>()->name() == oldValue) {
      return Identifier::create(node->span, node->scope, newValue);
    }

    return nullptr;
  }, {});
}

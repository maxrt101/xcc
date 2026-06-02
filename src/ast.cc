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

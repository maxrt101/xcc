#include "xcc/ast.h"
#include "xcc/lexer.h"
#include "xcc/util/log.h"
#include "xcc/util/string.h"

using namespace xcc;
using namespace xcc::ast;

static auto logger = xcc::util::log::Logger("AST");

static std::string operator_str_from_token_type(TokenType type) {
  switch (type) {
    case TokenType::TOKEN_PLUS:           return "+";
    case TokenType::TOKEN_MINUS:          return "-";
    case TokenType::TOKEN_SLASH:          return "/";
    case TokenType::TOKEN_STAR:           return "*";
    case TokenType::TOKEN_EQUALS:         return "=";
    case TokenType::TOKEN_EQUALS_EQUALS:  return "==";
    case TokenType::TOKEN_NOT_EQUALS:     return "!=";
    case TokenType::TOKEN_LESS:           return "<";
    case TokenType::TOKEN_GREATER:        return ">";
    case TokenType::TOKEN_LESS_EQUALS:    return "<=";
    case TokenType::TOKEN_GREATER_EQUALS: return ">=";
    case TokenType::TOKEN_AND:            return "&&";
    case TokenType::TOKEN_OR:             return "||";
    case TokenType::TOKEN_NOT:            return "!";
    case TokenType::TOKEN_AMP:            return "&";
    default:                              return "<?>";
  }
}

static void print_indent(int indent) {
  for (int i = 0; i < indent; ++i) {
    logger.print(" ");
  }
}

static bool node_any_of(NodeType type, const std::vector<NodeType>& match) {
  return std::any_of(match.begin(), match.end(), [&](auto t) { return t == type; });
}

static void print_node(Node* node, Node* parent, int indent) {
  if (!node) {
    return;
  }

  switch (node->type) {
    case AST_EXPR_NUMBER: {
      auto number = node->as<Number>();
      if (number->tag == Number::INTEGER) {
        logger.print("%lld", number->value.integer);
      } else {
        logger.print("%g", number->value.floating);
      }
      break;
    }

    case AST_EXPR_STRING: {
      auto str = util::strescseq(node->as<String>()->value, false);
      logger.print("\"%s\"", str.c_str());
      break;
    }

    case AST_EXPR_IDENTIFIER:
      logger.print("%s", node->as<Identifier>()->value.c_str());
      break;

    case AST_EXPR_CALL: {
      auto call = node->as<Call>();
      print_node(call->name.get(), parent, indent);
      logger.print("(");
      for (auto& arg : call->args) {
        print_node(arg.get(), call, indent);
        if (arg != call->args.back()) {
          logger.print(", ");
        }
      }
      logger.print(")");
      break;
    }

    case AST_EXPR_CAST: {
      auto cast = node->as<Cast>();
      print_node(cast->expr.get(), parent, indent);
      logger.print(" as ");
      print_node(cast->type.get(), parent, indent);
      break;
    }

    case AST_EXPR_BINARY: {
      auto cast = node->as<Binary>();
      print_node(cast->lhs.get(), cast, indent);
      logger.print(" %s ", operator_str_from_token_type(cast->operation.type).c_str());
      print_node(cast->rhs.get(), cast, indent);
      break;
    }

    case AST_EXPR_UNARY: {
      auto cast = node->as<Unary>();
      logger.print("%s", operator_str_from_token_type(cast->operation.type).c_str());
      print_node(cast->rhs.get(), cast, indent);
      break;
    }

    case AST_EXPR_SUBSCRIPT: {
      auto subscript = node->as<Subscript>();
      print_node(subscript->lhs.get(), subscript, indent);
      logger.print("[");
      print_node(subscript->rhs.get(), subscript, indent);
      logger.print("]");
      break;
    }

    case AST_EXPR_TYPE: {
      auto type = node->as<Type>();
      print_node(type->name.get(), type, indent);
      if (type->pointer) {
        logger.print("*");
      }
      break;
    }

    case AST_EXPR_TYPED_IDENTIFIER: {
      auto typed = node->as<TypedIdentifier>();
      print_node(typed->name.get(), typed, indent);
      logger.print(": ");
      print_node(typed->value_type.get(), typed, indent);
      if (typed->value) {
        logger.print(" = ");
        print_node(typed->value.get(), typed, indent);
      }
      break;
    }

    case AST_RETURN: {
      auto return_stmt = node->as<Return>();
      logger.print("return ");
      if (return_stmt->value) {
        print_node(return_stmt->value.get(), return_stmt, indent);
      }
      break;
    }

    case AST_EXPR_ASSIGN: {
      auto assign_expr = node->as<Assign>();
      print_node(assign_expr->lhs.get(), assign_expr, indent);
      logger.print(" = ");
      print_node(assign_expr->rhs.get(), assign_expr, indent);
      break;
    }

    case AST_BLOCK: {
      auto block = node->as<Block>();
      print_indent(indent);
      logger.print("{\n");
      for (auto& stmt : block->body) {
        print_indent(indent + 2);
        print_node(stmt.get(), block, indent + 2);
        if (!node_any_of(stmt->type, {AST_BLOCK, AST_FUNCTION_DECL, AST_FUNCTION_DEF, AST_IF, AST_FOR, AST_WHILE})) {
          logger.print(";\n");
        }
      }
      print_indent(indent);
      logger.print("}\n");
      break;
    }

    case AST_VAR_DECL: {
      auto vardecl = node->as<VarDecl>();
      logger.print("var ");
      print_node(vardecl->name.get(), vardecl, indent);
      logger.print(": ");
      print_node(vardecl->type.get(), vardecl, indent);
      if (vardecl->value) {
        logger.print(" = ");
        print_node(vardecl->value.get(), vardecl, indent);
      }
      break;
    }

    case AST_FUNCTION_DECL: {
      auto fndecl = node->as<FnDecl>();
      logger.print("fn ");
      print_node(fndecl->name.get(), fndecl, indent);
      logger.print("(");
      for (auto& arg : fndecl->args) {
        print_node(arg.get(), fndecl, indent);
        if (arg != fndecl->args.back()) {
          logger.print(", ");
        }
      }
      logger.print("): ");
      print_node(fndecl->return_type.get(), fndecl, indent);
      if (!parent->is(AST_FUNCTION_DEF)) {
        logger.print(";");
      }
      logger.print("\n");
      break;
    }

    case AST_FUNCTION_DEF: {
      auto fndef = node->as<FnDef>();
      print_node(fndef->decl.get(), fndef, indent);
      print_node(fndef->body.get(), fndef, indent);
      break;
    }

    case AST_IF: {
      auto if_stmt = node->as<If>();
      logger.print("if (");
      print_node(if_stmt->condition.get(), if_stmt, indent);
      logger.print(")\n");
      print_node(if_stmt->then_branch.get(), if_stmt, indent);
      if (if_stmt->else_branch) {
        print_indent(indent);
        logger.print("else\n");
        print_node(if_stmt->else_branch.get(), if_stmt, indent);
      }
      break;
    }

    case AST_FOR: {
      auto for_stmt = node->as<For>();
      logger.print("for (");
      print_node(for_stmt->init.get(), for_stmt, indent);
      logger.print("; ");
      print_node(for_stmt->cond.get(), for_stmt, indent);
      logger.print("; ");
      print_node(for_stmt->step.get(), for_stmt, indent);
      logger.print(")\n");
      print_node(for_stmt->body.get(), for_stmt, indent);
      break;
    }

    case AST_WHILE: {
      auto while_stmt = node->as<While>();
      logger.print("while (");
      print_node(while_stmt->condition.get(), while_stmt, indent);
      logger.print(")\n");
      print_node(while_stmt->body.get(), while_stmt, indent);
      break;
    }

    default:
      logger.print("Node<%d>", node->type);
      break;
  }
}

void xcc::ast::printAst(std::shared_ptr<Node> root) {
  print_node(root.get(), root.get(), 0);
}

bool xcc::ast::isOrIsLastInBlock(std::shared_ptr<Node> node, NodeType type) {
  if (node->is(type)) {
    return true;
  }

  if (node->is(AST_BLOCK)) {
    auto block = node->as<Block>();
    return block->body.back()->is(type);
  }

  return false;
}

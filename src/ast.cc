#include "xcc/ast.h"
#include "xcc/lexer.h"
#include "xcc/util/log.h"
#include "xcc/util/string.h"

using namespace xcc;
using namespace xcc::ast;

static auto logger = xcc::util::log::Logger("AST");

static void printIndent(int indent) {
  for (int i = 0; i < indent; ++i) {
    logger.print(" ");
  }
}

static void printNode(Node* node, Node* parent, int indent) {
  if (!node) {
    return;
  }

  switch (node->type) {
    case AST_EXPR_NUMBER: {
      auto number = node->as<Number>();
      if (number->tag == Number::INTEGER) {
        logger.print("{}", number->value.integer);
      } else {
        logger.print("{}", number->value.floating);
      }
      break;
    }

    case AST_EXPR_STRING: {
      auto str = util::strescseq(node->as<String>()->value, false);
      logger.print("\"{}\"", str);
      break;
    }

    case AST_EXPR_IDENTIFIER:
      logger.print("{}", node->as<Identifier>()->value);
      break;

    case AST_EXPR_CALL: {
      auto call = node->as<Call>();
      printNode(call->callee.get(), parent, indent);
      logger.print("(");
      for (auto& arg : call->args) {
        printNode(arg.get(), call, indent);
        if (arg != call->args.back()) {
          logger.print(", ");
        }
      }
      logger.print(")");
      break;
    }

    case AST_EXPR_CAST: {
      auto cast = node->as<Cast>();
      printNode(cast->expr.get(), parent, indent);
      logger.print(" as ");
      printNode(cast->type.get(), parent, indent);
      break;
    }

    case AST_EXPR_BINARY: {
      auto cast = node->as<Binary>();
      printNode(cast->lhs.get(), cast, indent);
      logger.print(" {} ", cast->operation.toString());
      printNode(cast->rhs.get(), cast, indent);
      break;
    }

    case AST_EXPR_UNARY: {
      auto cast = node->as<Unary>();
      logger.print("{}", cast->operation.toString());
      printNode(cast->rhs.get(), cast, indent);
      break;
    }

    case AST_EXPR_SUBSCRIPT: {
      auto subscript = node->as<Subscript>();
      printNode(subscript->lhs.get(), subscript, indent);
      logger.print("[");
      printNode(subscript->rhs.get(), subscript, indent);
      logger.print("]");
      break;
    }

    case AST_EXPR_TYPE: {
      auto type = node->as<Type>();
      printNode(type->name.get(), type, indent);
      if (type->pointer) {
        logger.print("*");
      }
      break;
    }

    case AST_EXPR_TYPED_IDENTIFIER: {
      auto typed = node->as<TypedIdentifier>();
      printNode(typed->name.get(), typed, indent);
      if (typed->value_type && typed->value_type->name) {
        logger.print(": ");
        printNode(typed->value_type.get(), typed, indent);
      }
      if (typed->value) {
        logger.print(" = ");
        printNode(typed->value.get(), typed, indent);
      }
      break;
    }

    case AST_EXPR_MEMBER_ACCESS: {
      auto access = node->as<MemberAccess>();
      printNode(access->lhs.get(), access, indent);
      access->kind == MemberAccess::MEMBER_ACCESS_VALUE ? logger.print(".") : logger.print("->");
      printNode(access->rhs.get(), access, indent);
      break;
    }

    case AST_RETURN: {
      auto return_stmt = node->as<Return>();
      logger.print("return ");
      if (return_stmt->value) {
        printNode(return_stmt->value.get(), return_stmt, indent);
      }
      break;
    }

    case AST_EXPR_ASSIGN: {
      auto assign = node->as<Assign>();
      printNode(assign->lhs.get(), assign, indent);
      logger.print(" {} ", assign->kind.toString());
      printNode(assign->rhs.get(), assign, indent);
      break;
    }

    case AST_BLOCK: {
      auto block = node->as<Block>();
      // printIndent(indent);
      logger.print(" {{\n");
      for (auto& stmt : block->body) {
        printIndent(indent + 2);
        printNode(stmt.get(), block, indent + 2);
        if (!stmt->isAnyOf(AST_BLOCK, AST_FUNCTION_DECL, AST_FUNCTION_DEF, AST_IF, AST_FOR, AST_WHILE, AST_STRUCT)) {
          logger.print(";\n");
        }
      }
      printIndent(indent);
      logger.print("}}");
      if (!parent->isAnyOf(AST_IF, AST_FOR, AST_WHILE)) {
        logger.print("\n");
      }
      break;
    }

    case AST_VAR_DECL: {
      auto vardecl = node->as<VarDecl>();
      logger.print("var ");
      printNode(vardecl->name.get(), vardecl, indent);
      if (vardecl->type && vardecl->type->name) {
        logger.print(": ");
        printNode(vardecl->type.get(), vardecl, indent);
      }
      if (vardecl->value) {
        logger.print(" = ");
        printNode(vardecl->value.get(), vardecl, indent);
      }
      break;
    }

    case AST_FUNCTION_DECL: {
      auto fndecl = node->as<FnDecl>();
      logger.print("fn ");
      printNode(fndecl->name.get(), fndecl, indent);
      logger.print("(");
      for (auto& arg : fndecl->args) {
        printNode(arg.get(), fndecl, indent);
        if (arg != fndecl->args.back()) {
          logger.print(", ");
        }
      }
      if (fndecl->isVariadic) {
        logger.print(", ...");
      }
      logger.print("): ");
      printNode(fndecl->return_type.get(), fndecl, indent);
      if (!parent->is(AST_FUNCTION_DEF)) {
        logger.print(";\n");
      }
      break;
    }

    case AST_FUNCTION_DEF: {
      auto fndef = node->as<FnDef>();
      printNode(fndef->decl.get(), fndef, indent);
      printNode(fndef->body.get(), fndef, indent);
      break;
    }

    case AST_STRUCT: {
      auto _struct = node->as<Struct>();
      logger.print("struct ");
      printNode(_struct->name.get(), _struct, indent);
      logger.print(" {{\n");
      for (auto& field : _struct->fields) {
        printIndent(indent + 2);
        printNode(field.get(), _struct, indent + 2);
        logger.print(";\n");
      }
      for (auto& method : _struct->methods) {
        printIndent(indent + 2);
        printNode(method.get(), _struct, indent + 2);
      }
      printIndent(indent);
      logger.print("}}\n");
      break;
    }

    case AST_IF: {
      auto if_stmt = node->as<If>();
      logger.print("if (");
      printNode(if_stmt->condition.get(), if_stmt, indent);
      logger.print(") ");
      printNode(if_stmt->then_branch.get(), if_stmt, indent);
      if (if_stmt->else_branch) {
        logger.print(" else");
        printNode(if_stmt->else_branch.get(), if_stmt, indent);
      }
      logger.print("\n");
      break;
    }

    case AST_FOR: {
      auto for_stmt = node->as<For>();
      logger.print("for (");
      printNode(for_stmt->init.get(), for_stmt, indent);
      logger.print("; ");
      printNode(for_stmt->cond.get(), for_stmt, indent);
      logger.print("; ");
      printNode(for_stmt->step.get(), for_stmt, indent);
      logger.print(") ");
      printNode(for_stmt->body.get(), for_stmt, indent);
      logger.print("\n");
      break;
    }

    case AST_WHILE: {
      auto while_stmt = node->as<While>();
      logger.print("while (");
      printNode(while_stmt->condition.get(), while_stmt, indent);
      logger.print(") ");
      printNode(while_stmt->body.get(), while_stmt, indent);
      break;
    }

    default:
      logger.print("Node<{}>", (int) node->type);
      break;
  }
}

void xcc::ast::printAst(std::shared_ptr<Node> root) {
  printNode(root.get(), root.get(), 0);
}

bool xcc::ast::isOrIsLastInBlock(std::shared_ptr<Node> node, NodeType type) {
  if (node->is(type)) {
    return true;
  }

  if (node->is(AST_BLOCK)) {
    auto block = node->as<Block>();
    return isOrIsLastInBlock(block->body.back(), type);
  }

  return false;
}

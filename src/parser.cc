#include <memory>

#include "xcc/parser.h"
#include "xcc/util/log.h"
#include "xcc/exceptions.h"

using namespace xcc;

static auto logger = xcc::util::log::Logger("PARSER");

bool Parser::isAtEnd() const {
  return current_idx >= tokens.size();
}

Token Parser::advance() {
  return tokens[current_idx++];
}

Token Parser::current() {
  return tokens[current_idx];
}

Token Parser::previous() {
  if (current_idx == 0) {
    throw ParserException("Tried to get previous token, while current is at index 0");
  }

  return tokens[current_idx-1];
}

Token Parser::next() {
  if (current_idx + 1 >= tokens.size()) {
    throw ParserException("Tried to get next token, which doesn't exist");
  }

  return tokens[current_idx+1];
}

bool Parser::check(TokenType expected) {
  return current().type == expected;
}

bool Parser::checkAdvance(TokenType expected) {
  if (current().is(expected)) {
    advance();
    return true;
  }

  return false;
}

std::shared_ptr<ast::Identifier> Parser::parseIdentifier(const std::string& ex_msg) {
  if (!checkAdvance(TOKEN_IDENTIFIER)) {
    throw ParserException(current().line, "Expected identifier " + ex_msg);
  }

  return ast::Identifier::create(previous().value);
}

std::shared_ptr<ast::Type> Parser::parseType() {
  auto id = parseIdentifier("for type name");
  // TODO: Check if ternary can be used without consequence
  if (checkAdvance(TOKEN_STAR)) {
    return ast::Type::create(ast::Node::cast(id), true);
  }
  return ast::Type::create(ast::Node::cast(id), false);
}

std::shared_ptr<ast::TypedIdentifier> Parser::parseValueDecl() {
  std::shared_ptr<ast::Identifier> name = parseIdentifier("for variable name");
  std::shared_ptr<ast::Type> type;
  std::shared_ptr<ast::Node> value;

  if (checkAdvance(TOKEN_COLON)) {
    type = parseType();
  }

  if (checkAdvance(TOKEN_EQUALS)) {
    value = parseExpr();
  }

  return ast::TypedIdentifier::create(name, type, value);
}

std::shared_ptr<ast::Node> Parser::parseFunction() {
  bool is_extern = false;
  bool is_variadic = false;

  if (check(TOKEN_EXTERN)) {
    advance();
    is_extern = true;
  }

  if (!checkAdvance(TOKEN_FN)) {
    throw ParserException(current().line, "Expected 'fn'");
  }

  auto name = parseIdentifier("for function name");
  std::vector<std::shared_ptr<ast::TypedIdentifier>> args;
  std::shared_ptr<ast::Type> return_type;

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    throw ParserException(current().line, "Expected '(' after function name");
  }

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      if (checkAdvance(TOKEN_3_DOTS)) {
        is_variadic = true;
        break;
      } else {
        args.push_back(parseValueDecl());
      }
    } while (checkAdvance(TOKEN_COMMA));
  }

  if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
    throw ParserException(current().line, "Expected ')' after function arguments");
  }

  if (!checkAdvance(TOKEN_COLON)) {
    throw ParserException(current().line, "Expected ':' after function arguments");
  }

  return_type = parseType();

  auto fndecl = ast::FnDecl::create(name, return_type, args, is_extern, is_variadic);

  if (!check(TOKEN_LEFT_BRACE)) {
    if (!checkAdvance(TOKEN_SEMICOLON)) {
      throw ParserException(current().line, "Expected ';' after function declaration");
    }
    return ast::Node::cast(fndecl);
  }

  auto body = parseBlock();

  return ast::FnDef::create(fndecl, body);
}

std::shared_ptr<ast::Block> Parser::parseBlock() {
  if (!checkAdvance(TOKEN_LEFT_BRACE)) {
    throw ParserException(current().line, "Expected '{'");
  }

  std::vector<std::shared_ptr<ast::Node>> nodes;

  bool shouldContinue = true;

  do {
    if (isAtEnd() || check(TOKEN_RIGHT_BRACE)) {
      break;
    }
    nodes.push_back(parseStmt());
    shouldContinue = previous().is(TOKEN_RIGHT_BRACE) || checkAdvance(TOKEN_SEMICOLON);
  } while (shouldContinue);

  if (!checkAdvance(TOKEN_RIGHT_BRACE)) {
    throw ParserException(current().line, "Expected '}'");
  }

  return ast::Block::create(nodes);
}

std::shared_ptr<ast::Node> Parser::parseVar(bool global) {
  if (!checkAdvance(TOKEN_VAR)) {
    throw ParserException(current().line, "Expected 'var'");
  }

  auto valdecl = parseValueDecl();

  return ast::VarDecl::create(valdecl->name, valdecl->value_type, valdecl->value, global);
}

std::shared_ptr<ast::Node> Parser::parseStruct() {
  if (!checkAdvance(TOKEN_STRUCT)) {
    throw ParserException(current().line, "Expected 'struct'");
  }

  auto name = parseIdentifier("for struct name");

  if (!checkAdvance(TOKEN_LEFT_BRACE)) {
    throw ParserException(current().line, "Expected '{' after 'struct'");
  }

  std::vector<std::shared_ptr<ast::TypedIdentifier>> fields;

  bool shouldContinue = true;

  do {
    if (isAtEnd() || check(TOKEN_RIGHT_BRACE)) {
      break;
    }
    fields.push_back(parseValueDecl());
    shouldContinue = previous().is(TOKEN_RIGHT_BRACE) || checkAdvance(TOKEN_SEMICOLON);
  } while (shouldContinue);

  if (!checkAdvance(TOKEN_RIGHT_BRACE)) {
    throw ParserException(current().line, "Expected '}' after struct definition");
  }

  return ast::Struct::create(name, fields);
}

std::shared_ptr<ast::Node> Parser::parseIf() {
  if (!checkAdvance(TOKEN_IF)) {
    throw ParserException(current().line, "Expected 'if'");
  }

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    throw ParserException(current().line, "Expected '(' after 'if'");
  }

  std::shared_ptr<ast::Node> cond = parseExpr();

  if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
    throw ParserException(current().line, "Expected ')' after 'if' condition");
  }

  std::shared_ptr<ast::Node> then_body = parseStmt();
  std::shared_ptr<ast::Node> else_body = nullptr;

  if (checkAdvance(TOKEN_ELSE)) {
    else_body = parseStmt();
  }

  return ast::If::create(cond, then_body, else_body);
}

std::shared_ptr<ast::Node> Parser::parseFor() {
    if (!checkAdvance(TOKEN_FOR)) {
    throw ParserException(current().line, "Expected 'for'");
  }

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    throw ParserException(current().line, "Expected '(' after 'for'");
  }

  std::shared_ptr<ast::Node> init = parseVar(false);

  if (!checkAdvance(TOKEN_SEMICOLON)) {
    throw ParserException(current().line, "Expected ';' after 'init' part of 'for'");
  }

  std::shared_ptr<ast::Node> cond = parseExpr();

  if (!checkAdvance(TOKEN_SEMICOLON)) {
    throw ParserException(current().line, "Expected ';' after 'cond' part of 'for'");
  }

  std::shared_ptr<ast::Node> step = parseExpr();

  if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
    throw ParserException(current().line, "Expected ')' after 'step' part of 'for'");
  }

  std::shared_ptr<ast::Node> body = parseStmt();

  return ast::For::create(ast::Node::cast<ast::VarDecl>(init), cond, step, body);
}

std::shared_ptr<ast::Node> Parser::parseWhile() {
  if (!checkAdvance(TOKEN_WHILE)) {
    throw ParserException(current().line, "Expected 'while'");
  }

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    throw ParserException(current().line, "Expected '(' after 'while'");
  }

  std::shared_ptr<ast::Node> cond = parseExpr();

  if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
    throw ParserException(current().line, "Expected ')' after 'if' condition");
  }

  std::shared_ptr<ast::Node> body = parseStmt();

  return ast::While::create(cond, body);
}

std::shared_ptr<ast::Node> Parser::parseReturn() {
  if (!checkAdvance(TOKEN_RETURN)) {
    throw ParserException(current().line, "Expected 'return'");
  }

  std::shared_ptr<ast::Node> expr;

  if (!check(TOKEN_SEMICOLON)) {
    expr = parseExpr();
  }

  return ast::Return::create(expr);
}

std::shared_ptr<ast::Node> Parser::parseStmt() {
  if (check(TOKEN_VAR)) {
    return parseVar(false);
  } else if (check(TOKEN_IF)) {
    return parseIf();
  } else if (check(TOKEN_FOR)) {
    return parseFor();
  } else if (check(TOKEN_WHILE)) {
    return parseWhile();
  } else if (check(TOKEN_RETURN)) {
    return parseReturn();
  } else if (check(TOKEN_LEFT_BRACE)) {
    return parseBlock();
  } else {
    return parseExpr();
  }
}

std::shared_ptr<ast::Node> Parser::parseExpr() {
  return parseAssignment();
}

std::shared_ptr<ast::Node> Parser::parseAssignment() {
  auto expr = parseLogicAndBitOps();

  while (checkAdvanceAnyOf(TOKEN_EQUALS, TOKEN_ADD_EQUALS, TOKEN_MIN_EQUALS, TOKEN_MUL_EQUALS, TOKEN_DIV_EQUALS,
                           TOKEN_AND_EQUALS, TOKEN_OR_EQUALS, TOKEN_LOGICAL_AND_EQUALS, TOKEN_LOGICAL_OR_EQUALS)) {
    Token op = previous();
    auto rhs = parseLogicAndBitOps();
    if (expr->isAnyOf(ast::AST_EXPR_IDENTIFIER, ast::AST_EXPR_UNARY, ast::AST_EXPR_SUBSCRIPT, ast::AST_EXPR_MEMBER_ACCESS)) {
      expr = ast::Assign::create(op, expr, rhs);
    } else {
      throw ParserException("Invalid LHS for assignment (" + ast::Node::typeToString(expr->type) + ")");
    }
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseLogicAndBitOps() {
  auto expr = parseEquality();

  while (checkAdvanceAnyOf(TOKEN_AND, TOKEN_OR, TOKEN_AMP, TOKEN_VERTICAL_LINE)) {
    Token op = previous();
    auto rhs = parseEquality();
    expr = ast::Binary::create(op, expr, rhs);
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseEquality() {
  auto expr = parseComparison();

  while (checkAdvanceAnyOf(TOKEN_EQUALS_EQUALS, TOKEN_NOT_EQUALS)) {
    Token op = previous();
    auto rhs = parseComparison();
    expr = ast::Binary::create(op, expr, rhs);
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseComparison() {
  auto expr = parseTerm();

  while (checkAdvanceAnyOf(TOKEN_GREATER, TOKEN_GREATER_EQUALS,
                               TOKEN_LESS, TOKEN_LESS_EQUALS)) {
    Token op = previous();
    auto rhs = parseTerm();
    expr = ast::Binary::create(op, expr, rhs);
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseTerm() {
  auto expr = parseFactor();

  while (checkAdvanceAnyOf(TOKEN_MINUS, TOKEN_PLUS)) {
    Token op = previous();
    auto rhs = parseFactor();
    expr = ast::Binary::create(op, expr, rhs);
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseFactor() {
  auto expr = parseCast();

  // TODO: %
  while (checkAdvanceAnyOf(TOKEN_SLASH, TOKEN_STAR)) {
    Token op = previous();
    auto rhs = parseCast();
    expr = ast::Binary::create(op, expr, rhs);
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseCast() {
  auto expr = parseUnary();

  if (checkAdvance(TOKEN_AS)) {
    return ast::Cast::create(expr, parseType());
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseUnary() {
  if (checkAdvanceAnyOf(TOKEN_NOT, TOKEN_MINUS,
                        TOKEN_AMP, TOKEN_STAR)) {
    Token op = previous();
    return ast::Unary::create(op, parseSubscript());
  }

  return parseSubscript();
}

std::shared_ptr<ast::Node> Parser::parseSubscript() {
  auto lhs = parseRvalue();

  if (checkAdvance(TOKEN_LEFT_SQUARE_BRACE)) {
    auto rhs = parseExpr();
    assertThrow(checkAdvance(TOKEN_RIGHT_SQUARE_BRACE), ParserException("Missing closing ']' after '[' in subscript operator"));
    return ast::Subscript::create(lhs, rhs);
  } else if (checkAdvance(TOKEN_DOT)) {
    return ast::MemberAccess::createByValue(lhs, parseIdentifier("for member access"));
  } else if (checkAdvance(TOKEN_RIGHT_ARROW)) {
    return ast::MemberAccess::createByPointer(lhs, parseIdentifier("for member access"));
  }

  return lhs;
}

std::shared_ptr<ast::Node> Parser::parseNumber() {
  if (previous().value.find('.') != std::string::npos) {
    return ast::Number::createFloating(std::stod(previous().value));
  } else {
    std::string value = previous().value;
    int base = 10;
    int prefix_len = 2;

    if (value.size() > 2 && value[0] == '0') {
      if (value[1] == 'x') {
        base = 16;
      } else if (value[1] == 'b') {
        base = 2;
      } else if (value[1] == 'o') {
        base = 8;
      } else {
        base = 8;
        prefix_len = 1;
      }
      value = value.substr(prefix_len);
    }

    return ast::Number::createInteger(std::stol(value, nullptr, base));
  }
}

std::shared_ptr<ast::Node> Parser::parseRvalue() {
  if (checkAdvance(TOKEN_NUMBER)) {
    return parseNumber();
  }

  if (checkAdvance(TOKEN_STRING)) {
    return ast::String::create(previous().value);
  }

  if (checkAdvance(TOKEN_CHAR)) {
    return ast::Number::createInteger(previous().value[0]);
  }

  if (checkAdvance(TOKEN_LEFT_PAREN)) {
    auto expr = parseExpr();
    if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
      throw ParserException(current().line, "Expected ')' after expression");
    }
    // TODO: ast::Group?
    return expr;
  }

  return parseLvalueAndCall();
}

std::shared_ptr<ast::Node> Parser::parseLvalueAndCall() {
  if (current().type != TOKEN_IDENTIFIER) {
    throw ParserException(current().line, "Unexpected token '" + current().value + "'(" + Token::typeToString(current().type) + "), expected identifier");
  }

  if (next().type == TOKEN_DOT) {
    auto lhs = parseIdentifier("for member access");
    advance();
    return ast::MemberAccess::createByValue(lhs, parseIdentifier("for member access"));
  } else if (next().type == TOKEN_RIGHT_ARROW) {
    auto lhs = parseIdentifier("for member access");
    advance();
    return ast::MemberAccess::createByPointer(lhs, parseIdentifier("for member access"));
  }

  // TODO: Fix this, what about dot separated sequences?
  if (next().type == TOKEN_LEFT_PAREN) {
    return parseCall();
  }

  return std::dynamic_pointer_cast<ast::Node>(parseIdentifier(""));
}


std::shared_ptr<ast::Node> Parser::parseCall() {
  auto name = parseIdentifier("(function call)");

  std::vector<std::shared_ptr<ast::Node>> args;

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    throw ParserException(current().line, "Expected '(' after function name (function call)");
  }

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      if (isAtEnd() || check(TOKEN_RIGHT_PAREN)) {
        break;
      }
      args.push_back(parseExpr());
    } while (checkAdvance(TOKEN_COMMA));
  }

  if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
    throw ParserException(current().line, "Expected ')' after function arguments (function call)");
  }

  return ast::Call::create(std::dynamic_pointer_cast<ast::Node>(name), args);
}

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens), current_idx(0) {}

std::shared_ptr<ast::Block> Parser::parse(bool isRepl) {
  auto block = ast::Block::create({});

  while (!isAtEnd()) {
    if (checkAnyOf(TOKEN_FN, TOKEN_EXTERN)) {
      block->body.push_back(parseFunction());
    } else if (check(TOKEN_VAR)) {
      block->body.push_back(parseVar(true));
      if (!checkAdvance(TOKEN_SEMICOLON)) {
        throw ParserException(current().line, "Expected ';' variable declaration (global scope)");
      }
    } else if (check(TOKEN_STRUCT)) {
      block->body.push_back(parseStruct());
    } else {
      if (isRepl) {
        block->body.push_back(parseStmt());
      } else {
        throw ParserException(current().line, "Unexpected token at top-level scope: '" + current().value + "' (" + Token::typeToString(current().type) + ")");
      }
    }
  }

  return block;
}


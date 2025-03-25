#pragma once

#include "xcc/ast.h"

namespace xcc {

class Parser {
  const std::vector<Token>& tokens;
  size_t current_idx;

private:
  bool isAtEnd() const;

  Token advance();
  Token previous();
  Token current();
  Token next();

  bool check(TokenType expected);
  bool checkAdvance(TokenType expected);

  template<typename... Types>
  inline bool checkAnyOf(Types... expected) {
    return current().isAnyOf(std::forward<Types>(expected)...);
  }

  template<typename... Types>
  inline bool checkAdvanceAnyOf(Types... expected) {
    if (current().isAnyOf(std::forward<Types>(expected)...)) {
      advance();
      return true;
    }

    return false;
  }

  std::shared_ptr<ast::Identifier> parseIdentifier(const std::string& ex_msg);
  std::shared_ptr<ast::Type> parseType();
  std::shared_ptr<ast::TypedIdentifier> parseValueDecl();

  std::shared_ptr<ast::Node> parseFunction();
  std::shared_ptr<ast::Block> parseBlock();
  std::shared_ptr<ast::Node> parseVar(bool global);
  std::shared_ptr<ast::Node> parseStruct();
  std::shared_ptr<ast::Node> parseIf();
  std::shared_ptr<ast::Node> parseFor();
  std::shared_ptr<ast::Node> parseWhile();
  std::shared_ptr<ast::Node> parseReturn();

  std::shared_ptr<ast::Node> parseStmt();
  std::shared_ptr<ast::Node> parseExpr();

  // Expressions
  std::shared_ptr<ast::Node> parseAssignment();
  std::shared_ptr<ast::Node> parseLogicAndBitOps();
  std::shared_ptr<ast::Node> parseEquality();
  std::shared_ptr<ast::Node> parseComparison();
  std::shared_ptr<ast::Node> parseTerm();
  std::shared_ptr<ast::Node> parseFactor();
  std::shared_ptr<ast::Node> parseCast();
  std::shared_ptr<ast::Node> parseCall();
  std::shared_ptr<ast::Node> parseUnary();
  std::shared_ptr<ast::Node> parseSubscript();
  std::shared_ptr<ast::Node> parseNumber();
  std::shared_ptr<ast::Node> parseRvalue();
  std::shared_ptr<ast::Node> parseLvalueAndCall();


public:
  explicit Parser(const std::vector<Token>& tokens);

  std::shared_ptr<ast::Block> parse(bool isRepl);
};


} /* namespace xcc */

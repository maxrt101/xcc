#pragma once

#include "xcc/ast.h"

namespace xcc {

/**
 * Parser context
 *
 * Parses token stream into an AST
 */
class Parser {
  /**
   * Context for Node (part of MemberAccess)
   */
  struct MemberAccessContext {
    std::shared_ptr<ast::Node> node;
    bool pointer;

    /**
     * Creates MemberAccess AST Node from 2 contexts
     */
    static std::shared_ptr<ast::MemberAccess> from(const MemberAccessContext& a, const MemberAccessContext& b);
  };

private:
  const std::vector<Token>& tokens;     /** Token stream */
  size_t current_idx;                   /** Index into `tokens` */
  std::vector<std::string> structStack; /** Stack of currently parsing struct definitions */

private:
  /**
   * Return true if current_idx is greater than tokens size
   */
  bool isAtEnd() const;

  /**
   * 'Advance' by 1 token. Increments current_idx and return token at current_idx-1
   */
  Token advance();

  /**
   * Returns previous token (at current_idx-1)
   */
  Token previous();

  /**
   * Returns current token (at current_idx) without advancing
   */
  Token current();

  /**
   * Returns current token (at current_idx+1) without advancing
   */
  Token next();

  /**
   * Returns true if current token has type `expected`
   */
  bool check(TokenType expected);

  /**
   * Returns true if current token has type `expected` and calls advance()
   * If token type doesn't match - doesn't advance and return false
   */
  bool checkAdvance(TokenType expected);

  /**
   * Returns true if current token has any type in `expected` list
   */
  template<typename... Types>
  bool checkAnyOf(Types... expected) {
    return current().isAnyOf(std::forward<Types>(expected)...);
  }

  /**
   * Returns true if current token has any type in `expected` list and calls advance()
   * If token type doesn't match - doesn't advance and return false
   */
  template<typename... Types>
  bool checkAdvanceAnyOf(Types... expected) {
    if (current().isAnyOf(std::forward<Types>(expected)...)) {
      advance();
      return true;
    }

    return false;
  }

  /**
   * Returns true if next token has type `expected`
   */
  bool checkNext(TokenType expected);

  // Helper
  std::shared_ptr<ast::Identifier> parseIdentifier(const std::string& ex_msg);
  std::shared_ptr<ast::Type> parseType();
  std::shared_ptr<ast::TypedIdentifier> parseValueDecl();

  // Statements
  std::shared_ptr<ast::Node> parseFunction(bool isMethod);
  std::shared_ptr<ast::Block> parseBlock();
  std::shared_ptr<ast::Node> parseVar(bool global);
  std::shared_ptr<ast::Node> parseStruct();
  std::shared_ptr<ast::Node> parseIf();
  std::shared_ptr<ast::Node> parseFor();
  std::shared_ptr<ast::Node> parseWhile();
  std::shared_ptr<ast::Node> parseReturn();

  // Generic
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
  std::shared_ptr<ast::Node> parseCall(std::shared_ptr<ast::Node> callee);
  std::shared_ptr<ast::Node> parseUnary();
  std::shared_ptr<ast::Node> parseSubscript();
  std::shared_ptr<ast::Node> parseNumber();
  std::shared_ptr<ast::Node> parseRvalue();
  std::shared_ptr<ast::Node> parseLvalueAndCall();

public:
  explicit Parser(const std::vector<Token>& tokens);

  /**
   * Performs parsing
   *
   * @param isRepl true if run in REPL mode
   */
  std::shared_ptr<ast::Block> parse(bool isRepl);
};


} /* namespace xcc */

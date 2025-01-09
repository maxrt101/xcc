#pragma once

#include <vector>
#include <string>

namespace xcc {

enum TokenType {
  TOKEN_EOF = 0,

  // Literals
  TOKEN_IDENTIFIER,
  TOKEN_NUMBER,
  TOKEN_STRING,

  // Keywords
  TOKEN_EXTERN,
  TOKEN_FN,
  TOKEN_VAR,
  TOKEN_IF,
  TOKEN_ELSE,
  TOKEN_FOR,
  TOKEN_WHILE,
  TOKEN_RETURN,
  TOKEN_AS,

  // Symbols/Tokens
  TOKEN_LEFT_BRACE,
  TOKEN_RIGHT_BRACE,
  TOKEN_LEFT_SQUARE_BRACE,
  TOKEN_RIGHT_SQUARE_BRACE,
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_SEMICOLON,
  TOKEN_EQUALS,
  TOKEN_PLUS,
  TOKEN_MINUS,
  TOKEN_SLASH,
  TOKEN_STAR,
  TOKEN_AMP,

  TOKEN_EQUALS_EQUALS,
  TOKEN_NOT_EQUALS,
  TOKEN_LESS,
  TOKEN_GREATER,
  TOKEN_LESS_EQUALS,
  TOKEN_GREATER_EQUALS,
  TOKEN_AND,
  TOKEN_OR,
  TOKEN_NOT,
};

struct Token {
  TokenType type;
  std::string value;
  size_t line;

  inline bool is(TokenType expected) const {
    return type == expected;
  }

  template <typename... Types>
  inline bool isAnyOf(Types... expected) const {
    return ((this->type == expected) || ...);
  }

  static std::string typeToString(TokenType type);
};

class Lexer {
  const std::string& text;
  size_t current_index = 0;
  size_t line = 1;
  std::vector<Token> result;

private:
  bool isAtEnd();
  size_t consume();
  char current();
  bool check(char expected);
  void skipWhitespace();

  void tokenizeString();
  void tokenizeIdentifier();
  void tokenizeNumber();

public:
  explicit Lexer(const std::string& text);

  std::vector<Token> tokenize();
};

} /* namespace xcc */

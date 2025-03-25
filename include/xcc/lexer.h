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
  TOKEN_CHAR,

  // Keywords
  TOKEN_EXTERN,
  TOKEN_FN,
  TOKEN_VAR,
  TOKEN_STRUCT,
  TOKEN_IF,
  TOKEN_ELSE,
  TOKEN_FOR,
  TOKEN_WHILE,
  TOKEN_RETURN,
  TOKEN_AS,

  // Braces/Parenthesis
  TOKEN_LEFT_BRACE,
  TOKEN_RIGHT_BRACE,
  TOKEN_LEFT_SQUARE_BRACE,
  TOKEN_RIGHT_SQUARE_BRACE,
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,

  // Special Operators
  TOKEN_COMMA,
  TOKEN_DOT,
  TOKEN_3_DOTS,
  TOKEN_COLON,
  TOKEN_SEMICOLON,
  TOKEN_RIGHT_ARROW,

  // Assignment Operators
  TOKEN_EQUALS,
  TOKEN_ADD_EQUALS,
  TOKEN_MIN_EQUALS,
  TOKEN_MUL_EQUALS,
  TOKEN_DIV_EQUALS,
  TOKEN_AND_EQUALS,
  TOKEN_OR_EQUALS,
  TOKEN_LOGICAL_AND_EQUALS,
  TOKEN_LOGICAL_OR_EQUALS,

  // Arithmetic Operators
  TOKEN_PLUS,
  TOKEN_MINUS,
  TOKEN_SLASH,
  TOKEN_STAR,
  TOKEN_AMP,
  TOKEN_VERTICAL_LINE,

  // Comparison/Logic operators
  TOKEN_EQUALS_EQUALS,
  TOKEN_NOT_EQUALS,
  TOKEN_LESS,
  TOKEN_GREATER,
  TOKEN_LESS_EQUALS,
  TOKEN_GREATER_EQUALS,
  TOKEN_AND,
  TOKEN_OR,
  TOKEN_NOT,

  TOKEN_COUNT,
};

struct Token {
  TokenType type;
  std::string value;
  size_t line;

  [[nodiscard]] inline bool is(TokenType expected) const {
    return type == expected;
  }

  template <typename... Types>
  [[nodiscard]] inline bool isAnyOf(Types... expected) const {
    return ((this->type == expected) || ...);
  }

  [[nodiscard]] inline Token clone(TokenType type) const {
    return {type, this->value, this->line};
  }

  [[nodiscard]] inline Token clone(const std::string& value) const {
    return {this->type, value, this->line};
  }

  [[nodiscard]] inline Token clone(size_t line) const {
    return {this->type, this->value, line};
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
  void tokenizeChar();
  void tokenizeIdentifier();
  void tokenizeNumber();

public:
  explicit Lexer(const std::string& text);

  std::vector<Token> tokenize();
};

} /* namespace xcc */

#pragma once

#include <vector>
#include <string>

namespace xcc {

/**
 * Token type
 */
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

/**
 * Token - smallest atomic part of source
 */
struct Token {
  TokenType type;     /** Token type */
  std::string value;  /** Token value (for identifier/string/number/char) */
  size_t line;        /** Line is source code */

  /**
   * Returns true if token is of type `expected`
   */
  [[nodiscard]] inline bool is(TokenType expected) const {
    return type == expected;
  }

  /**
   * Returns true if token is of any type in `expected` list
   */
  template <typename... Types>
  [[nodiscard]] inline bool isAnyOf(Types... expected) const {
    return ((this->type == expected) || ...);
  }

  /**
   * Clones token (creates a new one) changing original type
   */
  [[nodiscard]] inline Token clone(TokenType type) const {
    return {type, this->value, this->line};
  }

  /**
   * Clones token (creates a new one) changing original value
   */
  [[nodiscard]] inline Token clone(const std::string& value) const {
    return {this->type, value, this->line};
  }

  /**
   * Clones token (creates a new one) changing original line
   */
  [[nodiscard]] inline Token clone(size_t line) const {
    return {this->type, this->value, line};
  }

  /**
   * Returns string representation of a token
   */
  std::string toString() const;

  /**
   * Converts token type to string
   */
  static std::string typeToString(TokenType type);
};

/**
 * Lexer/Tokenizer context
 *
 * User PrefixTree for a compact and efficient token lookup
 */
class Lexer {
  const std::string& text;    /** Source code reference */
  size_t current_index = 0;   /** Index into `text` */
  size_t line = 1;            /** Line counter */
  std::vector<Token> result;  /** Resulting token stream */

private:
  /**
   * Return true if current_index is greater than text size
   */
  bool isAtEnd();

  /**
   * 'Consumes' a token. Advances current_index and returns char at current_index-1
   */
  size_t consume();

  /**
   * Returns current char (at current_index)
   */
  char current();

  /**
   * Checks if current char (at current_index) is same as `expected`
   */
  bool check(char expected);

  /**
   * Skip whitespace from current until next non-whitespace char
   */
  void skipWhitespace();

  /**
   * Tokenizes a string literal
   */
  void tokenizeString();

  /**
   * Tokenizes a character literal
   */
  void tokenizeChar();

  /**
   * Tokenizes an identifier
   */
  void tokenizeIdentifier();

  /**
   * Tokenizes a number literal (float, 10 & 16 base ints)
   */
  void tokenizeNumber();

public:
  explicit Lexer(const std::string& text);

  /**
   * Performs tokenization
   */
  std::vector<Token> tokenize();
};

} /* namespace xcc */

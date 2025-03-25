#include "xcc/lexer.h"
#include "xcc/exceptions.h"
#include "xcc/util/prefix_tree.h"
#include "xcc/util/string.h"

using namespace xcc;

static PrefixTree<TokenType> s_token_patterns(TOKEN_EOF, {
    {"extern", TOKEN_EXTERN},
    {"fn", TOKEN_FN},
    {"var", TOKEN_VAR},
    {"struct", TOKEN_STRUCT},
    {"if", TOKEN_IF},
    {"else", TOKEN_ELSE},
    {"for", TOKEN_FOR},
    {"while", TOKEN_WHILE},
    {"return", TOKEN_RETURN},
    {"as", TOKEN_AS},
    {"{", TOKEN_LEFT_BRACE},
    {"}", TOKEN_RIGHT_BRACE},
    {"[", TOKEN_LEFT_SQUARE_BRACE},
    {"]", TOKEN_RIGHT_SQUARE_BRACE},
    {"(", TOKEN_LEFT_PAREN},
    {")", TOKEN_RIGHT_PAREN},
    {",", TOKEN_COMMA},
    {".", TOKEN_DOT},
    {"...", TOKEN_3_DOTS},
    {":", TOKEN_COLON},
    {";", TOKEN_SEMICOLON},
    {"->", TOKEN_RIGHT_ARROW},
    {"=", TOKEN_EQUALS},
    {"+=", TOKEN_ADD_EQUALS},
    {"-=", TOKEN_MIN_EQUALS},
    {"*=", TOKEN_MUL_EQUALS},
    {"/=", TOKEN_DIV_EQUALS},
    {"&=", TOKEN_AND_EQUALS},
    {"|=", TOKEN_OR_EQUALS},
    {"&&=", TOKEN_LOGICAL_AND_EQUALS},
    {"||=", TOKEN_LOGICAL_OR_EQUALS},
    {"+", TOKEN_PLUS},
    {"-", TOKEN_MINUS},
    {"/", TOKEN_SLASH},
    {"*", TOKEN_STAR},
    {"&", TOKEN_AMP},
    {"|", TOKEN_VERTICAL_LINE},
    {"==", TOKEN_EQUALS_EQUALS},
    {"!=", TOKEN_NOT_EQUALS},
    {"<", TOKEN_LESS},
    {"<=", TOKEN_LESS_EQUALS},
    {">", TOKEN_GREATER},
    {">=", TOKEN_GREATER_EQUALS},
    {"&&", TOKEN_AND},
    {"||", TOKEN_OR},
    {"!", TOKEN_NOT},
});

static inline bool isBase16Char(char c) {
  return isnumber(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

std::string Token::typeToString(TokenType type) {
  static std::unordered_map<TokenType, std::string> type_map {
      {TOKEN_EOF, "TOKEN_EOF"},
      {TOKEN_IDENTIFIER, "TOKEN_IDENTIFIER"},
      {TOKEN_NUMBER, "TOKEN_NUMBER"},
      {TOKEN_STRING, "TOKEN_STRING"},
      {TOKEN_CHAR, "TOKEN_CHAR"},
      {TOKEN_EXTERN, "TOKEN_EXTERN"},
      {TOKEN_FN, "TOKEN_FN"},
      {TOKEN_VAR, "TOKEN_VAR"},
      {TOKEN_STRUCT, "TOKEN_STRUCT"},
      {TOKEN_IF, "TOKEN_IF"},
      {TOKEN_ELSE, "TOKEN_ELSE"},
      {TOKEN_FOR, "TOKEN_FOR"},
      {TOKEN_WHILE, "TOKEN_WHILE"},
      {TOKEN_RETURN, "TOKEN_RETURN"},
      {TOKEN_AS, "TOKEN_AS"},
      {TOKEN_LEFT_BRACE, "TOKEN_LEFT_BRACE"},
      {TOKEN_RIGHT_BRACE, "TOKEN_RIGHT_BRACE"},
      {TOKEN_LEFT_SQUARE_BRACE, "TOKEN_LEFT_SQUARE_BRACE"},
      {TOKEN_RIGHT_SQUARE_BRACE, "TOKEN_RIGHT_SQUARE_BRACE"},
      {TOKEN_LEFT_PAREN, "TOKEN_LEFT_PAREN"},
      {TOKEN_RIGHT_PAREN, "TOKEN_RIGHT_PAREN"},
      {TOKEN_COMMA, "TOKEN_COMMA"},
      {TOKEN_COLON, "TOKEN_COLON"},
      {TOKEN_DOT, "TOKEN_DOT"},
      {TOKEN_3_DOTS, "TOKEN_3_DOTS"},
      {TOKEN_SEMICOLON, "TOKEN_SEMICOLON"},
      {TOKEN_EQUALS, "TOKEN_EQUALS"},
      {TOKEN_ADD_EQUALS, "TOKEN_ADD_EQUALS"},
      {TOKEN_MIN_EQUALS, "TOKEN_MIN_EQUALS"},
      {TOKEN_MUL_EQUALS, "TOKEN_MUL_EQUALS"},
      {TOKEN_DIV_EQUALS, "TOKEN_DIV_EQUALS"},
      {TOKEN_AND_EQUALS, "TOKEN_AND_EQUALS"},
      {TOKEN_OR_EQUALS, "TOKEN_OR_EQUALS"},
      {TOKEN_LOGICAL_AND_EQUALS, "TOKEN_LOGICAL_AND_EQUALS"},
      {TOKEN_LOGICAL_OR_EQUALS, "TOKEN_LOGICAL_OR_EQUALS"},
      {TOKEN_PLUS, "TOKEN_PLUS"},
      {TOKEN_MINUS, "TOKEN_MINUS"},
      {TOKEN_SLASH, "TOKEN_SLASH"},
      {TOKEN_STAR, "TOKEN_STAR"},
      {TOKEN_AMP, "TOKEN_AMP"},
      {TOKEN_VERTICAL_LINE, "TOKEN_VERTICAL_LINE"},
      {TOKEN_EQUALS_EQUALS, "TOKEN_EQUALS_EQUALS"},
      {TOKEN_NOT_EQUALS, "TOKEN_NOT_EQUALS"},
      {TOKEN_LESS, "TOKEN_LESS"},
      {TOKEN_GREATER, "TOKEN_GREATER"},
      {TOKEN_LESS_EQUALS, "TOKEN_LESS_EQUALS"},
      {TOKEN_GREATER_EQUALS, "TOKEN_GREATER_EQUALS"},
      {TOKEN_AND, "TOKEN_AND"},
      {TOKEN_OR, "TOKEN_OR"},
      {TOKEN_NOT, "TOKEN_NOT"},
  };

  if (type_map.find(type) != type_map.end()) {
    return type_map[type];
  }

  return "UNKNOWN";
}

bool Lexer::isAtEnd() {
  return current_index >= text.size();
}

size_t Lexer::consume() {
  return ++current_index;
}

char Lexer::current() {
  return text[current_index];
}

bool Lexer::check(char expected) {
  return current() == expected;
}

void Lexer::skipWhitespace() {
  while (check(' ')
      || check('\n')
      || check('\t')
      || check('\r')
      || check('#')
  ) {
    if (isAtEnd()) {
      return;
    }

    if (check('\n')) {
      ++line;
    }

    if (check('#')) {
      while (!isAtEnd() && !check('\n')) {
        consume();
      }
      ++line;
    }

    consume();
  }
}

void Lexer::tokenizeString() {
  size_t start = consume();
  while (!check('"')) {
    if (isAtEnd()) {
      throw LexerException(line, "Unexpected EOF");
    }

    if (check('\n')) {
      ++line;
    }

    consume();
  }

  result.push_back({TOKEN_STRING, util::strescseq(text.substr(start, current_index - start), true)});

  // Skip closing quote
  consume();
}

void Lexer::tokenizeChar() {
  // Skip opening quote
  consume();

  result.push_back({TOKEN_CHAR, util::strescseq(std::string() + current(), true)});
  consume();

  assertThrow(check('\''), LexerException("Expected closing quote after char literal"));

  // Skip closing quote
  consume();
}

void Lexer::tokenizeIdentifier() {
  size_t begin = current_index;

  while (isalnum(current()) || check('_')) {
    if (isAtEnd()) {
      throw LexerException(line, "Unexpected EOF");
    }

    consume();
  }

  result.push_back({TOKEN_IDENTIFIER, text.substr(begin, current_index - begin), line});
}

void Lexer::tokenizeNumber() {
  size_t begin = current_index;
  bool allow_base_16_chars = false;

  if (current() == '0') {
    consume();

    switch (current()) {
      case 'x':
        allow_base_16_chars = true;
      case 'b':
        consume();
      default:
        break;
    }
  }

  while (isnumber(current())
         || current() == '.'
         || allow_base_16_chars && isBase16Char(current())) {
    if (isAtEnd()) {
      throw LexerException(line, "Unexpected EOF");
    }

    consume();
  }

  result.push_back({TOKEN_NUMBER, text.substr(begin, current_index - begin), line});
}

Lexer::Lexer(const std::string &text) : text(text) {}

std::vector<Token> Lexer::tokenize() {
  while (!isAtEnd()) {
    skipWhitespace();

    if (isAtEnd()) {
      break;
    }

    auto [token_type, token_size] = s_token_patterns.find(text, current_index);

    if (token_type != TOKEN_EOF) {
      current_index += token_size;
      result.push_back({token_type, "", line});
      continue;
    }

    if (isalpha(current()) || check('_')) {
      tokenizeIdentifier();
    } else if (check('"')) {
      tokenizeString();
    } else if (check('\'')) {
      tokenizeChar();
    } else if (isnumber(current())) {
      // TODO: add hex, floating point, octal, bin, etc
      tokenizeNumber();
    }
  }

  return result;
}

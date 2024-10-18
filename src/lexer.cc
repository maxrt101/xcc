#include "xcc/lexer.h"
#include "xcc/exceptions.h"
#include "xcc/util/prefix_tree.h"

using namespace xcc;

static PrefixTree<TokenType> s_token_patterns(TOKEN_EOF, {
    {"extern", TOKEN_EXTERN},
    {"fn", TOKEN_FN},
    {"var", TOKEN_VAR},
    {"if", TOKEN_IF},
    {"else", TOKEN_ELSE},
    {"for", TOKEN_FOR},
    {"while", TOKEN_WHILE},
    {"return", TOKEN_RETURN},
    {"as", TOKEN_AS},
    {"{", TOKEN_LEFT_BRACE},
    {"}", TOKEN_RIGHT_BRACE},
    {"(", TOKEN_LEFT_PAREN},
    {")", TOKEN_RIGHT_PAREN},
    {",", TOKEN_COMMA},
    {":", TOKEN_COLON},
    {";", TOKEN_SEMICOLON},
    {"=", TOKEN_EQUALS},
    {"+", TOKEN_PLUS},
    {"-", TOKEN_MINUS},
    {"/", TOKEN_SLASH},
    {"*", TOKEN_STAR},
    {"&", TOKEN_AMP},
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

std::string Token::typeToString(TokenType type) {
  static std::unordered_map<TokenType, std::string> type_map {
      {TOKEN_EOF, "TOKEN_EOF"},
      {TOKEN_IDENTIFIER, "TOKEN_IDENTIFIER"},
      {TOKEN_NUMBER, "TOKEN_NUMBER"},
      {TOKEN_STRING, "TOKEN_STRING"},
      {TOKEN_EXTERN, "TOKEN_EXTERN"},
      {TOKEN_FN, "TOKEN_FN"},
      {TOKEN_VAR, "TOKEN_VAR"},
      {TOKEN_IF, "TOKEN_IF"},
      {TOKEN_ELSE, "TOKEN_ELSE"},
      {TOKEN_FOR, "TOKEN_FOR"},
      {TOKEN_WHILE, "TOKEN_WHILE"},
      {TOKEN_RETURN, "TOKEN_RETURN"},
      {TOKEN_AS, "TOKEN_AS"},
      {TOKEN_LEFT_BRACE, "TOKEN_LEFT_BRACE"},
      {TOKEN_RIGHT_BRACE, "TOKEN_RIGHT_BRACE"},
      {TOKEN_LEFT_PAREN, "TOKEN_LEFT_PAREN"},
      {TOKEN_RIGHT_PAREN, "TOKEN_RIGHT_PAREN"},
      {TOKEN_COMMA, "TOKEN_COMMA"},
      {TOKEN_COLON, "TOKEN_COLON"},
      {TOKEN_SEMICOLON, "TOKEN_SEMICOLON"},
      {TOKEN_EQUALS, "TOKEN_EQUALS"},
      {TOKEN_PLUS, "TOKEN_PLUS"},
      {TOKEN_MINUS, "TOKEN_MINUS"},
      {TOKEN_SLASH, "TOKEN_SLASH"},
      {TOKEN_STAR, "TOKEN_STAR"},
      {TOKEN_AMP, "TOKEN_AMP"},
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

  result.push_back({TOKEN_STRING, text.substr(start, current_index - start)});

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

  while (isnumber(current()) || current() == '.') {
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
    } else if (isnumber(current())) {
      // TODO: add hex, floating point, octal, bin, etc
      tokenizeNumber();
    }
  }

  return result;
}

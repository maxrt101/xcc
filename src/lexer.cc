#include "xcc/lexer.h"
#include "xcc/util/log.h"
#include "xcc/exceptions.h"
#include "xcc/util/prefix_tree.h"
#include "xcc/util/string.h"

using namespace xcc;

static auto logger = util::log::Logger("LEXER");

static const PrefixTree<TokenType> s_token_patterns(TOKEN_EOF, {
    {"extern",  TOKEN_EXTERN},
    {"fn",      TOKEN_FN},
    {"var",     TOKEN_VAR},
    {"struct",  TOKEN_STRUCT},
    {"if",      TOKEN_IF},
    {"else",    TOKEN_ELSE},
    {"for",     TOKEN_FOR},
    {"while",   TOKEN_WHILE},
    {"return",  TOKEN_RETURN},
    {"as",      TOKEN_AS},
    {"self",    TOKEN_SELF},
    {"use",     TOKEN_USE},
    {"mod",     TOKEN_MOD},
    {"type",    TOKEN_TYPE},
    {"macro",   TOKEN_MACRO},
    {"{",       TOKEN_LEFT_BRACE},
    {"}",       TOKEN_RIGHT_BRACE},
    {"[",       TOKEN_LEFT_SQUARE_BRACE},
    {"]",       TOKEN_RIGHT_SQUARE_BRACE},
    {"(",       TOKEN_LEFT_PAREN},
    {")",       TOKEN_RIGHT_PAREN},
    {",",       TOKEN_COMMA},
    {".",       TOKEN_DOT},
    {"...",     TOKEN_3_DOTS},
    {":",       TOKEN_COLON},
    {";",       TOKEN_SEMICOLON},
    {"->",      TOKEN_RIGHT_ARROW},
    {"$",       TOKEN_DOLLAR_SIGN},
    {"::",      TOKEN_SCOPE},
    {"=",       TOKEN_EQUALS},
    {"+=",      TOKEN_ADD_EQUALS},
    {"-=",      TOKEN_MIN_EQUALS},
    {"*=",      TOKEN_MUL_EQUALS},
    {"/=",      TOKEN_DIV_EQUALS},
    {"&=",      TOKEN_AND_EQUALS},
    {"|=",      TOKEN_OR_EQUALS},
    {"&&=",     TOKEN_LOGICAL_AND_EQUALS},
    {"||=",     TOKEN_LOGICAL_OR_EQUALS},
    {"+",       TOKEN_PLUS},
    {"-",       TOKEN_MINUS},
    {"/",       TOKEN_SLASH},
    {"*",       TOKEN_STAR},
    {"&",       TOKEN_AMP},
    {"|",       TOKEN_VERTICAL_LINE},
    {"==",      TOKEN_EQUALS_EQUALS},
    {"!=",      TOKEN_NOT_EQUALS},
    {"<",       TOKEN_LESS},
    {"<=",      TOKEN_LESS_EQUALS},
    {">",       TOKEN_GREATER},
    {">=",      TOKEN_GREATER_EQUALS},
    {"&&",      TOKEN_AND},
    {"||",      TOKEN_OR},
    {"!",       TOKEN_NOT},
});

static const std::unordered_map<TokenType, std::string> s_token_type_name_map {
    {TOKEN_EOF,                 "TOKEN_EOF"},
    {TOKEN_IDENTIFIER,          "TOKEN_IDENTIFIER"},
    {TOKEN_NUMBER,              "TOKEN_NUMBER"},
    {TOKEN_STRING,              "TOKEN_STRING"},
    {TOKEN_CHAR,                "TOKEN_CHAR"},
    {TOKEN_EXTERN,              "TOKEN_EXTERN"},
    {TOKEN_FN,                  "TOKEN_FN"},
    {TOKEN_VAR,                 "TOKEN_VAR"},
    {TOKEN_STRUCT,              "TOKEN_STRUCT"},
    {TOKEN_IF,                  "TOKEN_IF"},
    {TOKEN_ELSE,                "TOKEN_ELSE"},
    {TOKEN_FOR,                 "TOKEN_FOR"},
    {TOKEN_WHILE,               "TOKEN_WHILE"},
    {TOKEN_RETURN,              "TOKEN_RETURN"},
    {TOKEN_AS,                  "TOKEN_AS"},
    {TOKEN_SELF,                "TOKEN_SELF"},
    {TOKEN_USE,                 "TOKEN_USE"},
    {TOKEN_MOD,                 "TOKEN_MOD"},
    {TOKEN_TYPE,                "TOKEN_TYPE"},
    {TOKEN_MACRO,               "TOKEN_MACRO"},
    {TOKEN_LEFT_BRACE,          "TOKEN_LEFT_BRACE"},
    {TOKEN_RIGHT_BRACE,         "TOKEN_RIGHT_BRACE"},
    {TOKEN_LEFT_SQUARE_BRACE,   "TOKEN_LEFT_SQUARE_BRACE"},
    {TOKEN_RIGHT_SQUARE_BRACE,  "TOKEN_RIGHT_SQUARE_BRACE"},
    {TOKEN_LEFT_PAREN,          "TOKEN_LEFT_PAREN"},
    {TOKEN_RIGHT_PAREN,         "TOKEN_RIGHT_PAREN"},
    {TOKEN_COMMA,               "TOKEN_COMMA"},
    {TOKEN_COLON,               "TOKEN_COLON"},
    {TOKEN_DOT,                 "TOKEN_DOT"},
    {TOKEN_3_DOTS,              "TOKEN_3_DOTS"},
    {TOKEN_SEMICOLON,           "TOKEN_SEMICOLON"},
    {TOKEN_RIGHT_ARROW,         "TOKEN_RIGHT_ARROW"},
    {TOKEN_DOLLAR_SIGN,         "TOKEN_DOLLAR_SIGN"},
    {TOKEN_SCOPE,               "TOKEN_SCOPE"},
    {TOKEN_EQUALS,              "TOKEN_EQUALS"},
    {TOKEN_ADD_EQUALS,          "TOKEN_ADD_EQUALS"},
    {TOKEN_MIN_EQUALS,          "TOKEN_MIN_EQUALS"},
    {TOKEN_MUL_EQUALS,          "TOKEN_MUL_EQUALS"},
    {TOKEN_DIV_EQUALS,          "TOKEN_DIV_EQUALS"},
    {TOKEN_AND_EQUALS,          "TOKEN_AND_EQUALS"},
    {TOKEN_OR_EQUALS,           "TOKEN_OR_EQUALS"},
    {TOKEN_LOGICAL_AND_EQUALS,  "TOKEN_LOGICAL_AND_EQUALS"},
    {TOKEN_LOGICAL_OR_EQUALS,   "TOKEN_LOGICAL_OR_EQUALS"},
    {TOKEN_PLUS,                "TOKEN_PLUS"},
    {TOKEN_MINUS,               "TOKEN_MINUS"},
    {TOKEN_SLASH,               "TOKEN_SLASH"},
    {TOKEN_STAR,                "TOKEN_STAR"},
    {TOKEN_AMP,                 "TOKEN_AMP"},
    {TOKEN_VERTICAL_LINE,       "TOKEN_VERTICAL_LINE"},
    {TOKEN_EQUALS_EQUALS,       "TOKEN_EQUALS_EQUALS"},
    {TOKEN_NOT_EQUALS,          "TOKEN_NOT_EQUALS"},
    {TOKEN_LESS,                "TOKEN_LESS"},
    {TOKEN_GREATER,             "TOKEN_GREATER"},
    {TOKEN_LESS_EQUALS,         "TOKEN_LESS_EQUALS"},
    {TOKEN_GREATER_EQUALS,      "TOKEN_GREATER_EQUALS"},
    {TOKEN_AND,                 "TOKEN_AND"},
    {TOKEN_OR,                  "TOKEN_OR"},
    {TOKEN_NOT,                 "TOKEN_NOT"},
};

static const std::unordered_map<TokenType, std::string> s_token_type_value_map {
    {TOKEN_EOF,                 "EOF"},
    {TOKEN_IDENTIFIER,          "IDENTIFIER"},
    {TOKEN_NUMBER,              "NUMBER"},
    {TOKEN_STRING,              "STRING"},
    {TOKEN_CHAR,                "CHAR"},
    {TOKEN_EXTERN,              "extern"},
    {TOKEN_FN,                  "fn"},
    {TOKEN_VAR,                 "var"},
    {TOKEN_STRUCT,              "struct"},
    {TOKEN_IF,                  "if"},
    {TOKEN_ELSE,                "else"},
    {TOKEN_FOR,                 "for"},
    {TOKEN_WHILE,               "while"},
    {TOKEN_RETURN,              "return"},
    {TOKEN_AS,                  "as"},
    {TOKEN_SELF,                "self"},
    {TOKEN_USE,                 "use"},
    {TOKEN_MOD,                 "mod"},
    {TOKEN_TYPE,                "type"},
    {TOKEN_MACRO,               "macro"},
    {TOKEN_LEFT_BRACE,          "{"},
    {TOKEN_RIGHT_BRACE,         "}"},
    {TOKEN_LEFT_SQUARE_BRACE,   "["},
    {TOKEN_RIGHT_SQUARE_BRACE,  "]"},
    {TOKEN_LEFT_PAREN,          "("},
    {TOKEN_RIGHT_PAREN,         ")"},
    {TOKEN_COMMA,               ","},
    {TOKEN_COLON,               ":"},
    {TOKEN_DOT,                 "."},
    {TOKEN_3_DOTS,              "..."},
    {TOKEN_SEMICOLON,           ";"},
    {TOKEN_RIGHT_ARROW,         "->"},
    {TOKEN_DOLLAR_SIGN,         "$"},
    {TOKEN_SCOPE,               "::"},
    {TOKEN_EQUALS,              "="},
    {TOKEN_ADD_EQUALS,          "+="},
    {TOKEN_MIN_EQUALS,          "-="},
    {TOKEN_MUL_EQUALS,          "*="},
    {TOKEN_DIV_EQUALS,          "/="},
    {TOKEN_AND_EQUALS,          "&="},
    {TOKEN_OR_EQUALS,           "|="},
    {TOKEN_LOGICAL_AND_EQUALS,  "&&="},
    {TOKEN_LOGICAL_OR_EQUALS,   "||="},
    {TOKEN_PLUS,                "+"},
    {TOKEN_MINUS,               "-"},
    {TOKEN_SLASH,               "/"},
    {TOKEN_STAR,                "*"},
    {TOKEN_AMP,                 "&"},
    {TOKEN_VERTICAL_LINE,       "|"},
    {TOKEN_EQUALS_EQUALS,       "=="},
    {TOKEN_NOT_EQUALS,          "!="},
    {TOKEN_LESS,                "<"},
    {TOKEN_GREATER,             ">"},
    {TOKEN_LESS_EQUALS,         "<="},
    {TOKEN_GREATER_EQUALS,      ">="},
    {TOKEN_AND,                 "&&"},
    {TOKEN_OR,                  "||"},
    {TOKEN_NOT,                 "!"},
};

static bool isBase16Char(char c) {
  return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

std::string Token::toString() const {
  switch (type) {
    case TOKEN_IDENTIFIER:  [[fallthrough]];
    case TOKEN_CHAR:        [[fallthrough]];
    case TOKEN_NUMBER:
      return value;

    case TOKEN_STRING:
      return "\"" + value + "\"";

    default:
      return s_token_type_value_map.at(type);
  }
}

std::string Token::typeToString(TokenType type) {
  if (s_token_type_name_map.find(type) != s_token_type_name_map.end()) {
    return s_token_type_name_map.at(type);
  }

  return "UNKNOWN";
}

static bool isalnumstr(std::string s) {
  for (auto& c : s) {
    if (!isalnum(c)) {
      return false;
    }
  }

  return true;
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

bool Lexer::isIdentifierChar(char c) {
  return isalnum(c) || c == '_';
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
      Error(ERROR_UNEXPECTED_EOF, {fileId, start, current_index - start}, "").raise();
    }

    if (check('\n')) {
      ++line;
    }

    consume();
  }

  auto size = current_index - start;

  result.push_back({
    TOKEN_STRING,
    util::strescseq(text.substr(start, size), true),
    {fileId, current_index, size}
  });

  // Skip closing quote
  consume();
}

void Lexer::tokenizeChar() {
  // Skip opening quote
  consume();

  result.push_back({
    TOKEN_CHAR,
    util::strescseq(std::string() + current(), true),
    {fileId, current_index-1, 3}
  });

  consume();

  if (!check('\'')) {
    Error(ERROR_MISSING_CLOSING_QUOTE, {fileId, current_index, 1}, "Expected closing quote after char literal").raise();
  }

  // Skip closing quote
  consume();
}

void Lexer::tokenizeIdentifier() {
  size_t begin = current_index;

  while (isIdentifierChar(current())) {
    if (isAtEnd()) {
      Error(ERROR_UNEXPECTED_EOF, {fileId, begin, current_index - begin}, "").raise();
    }

    consume();
  }

  auto size = current_index - begin;

  result.push_back({
    TOKEN_IDENTIFIER,
    text.substr(begin, size),
    {fileId, begin, size}
  });
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

  while (isdigit(current())
         || current() == '.'
         || allow_base_16_chars && isBase16Char(current())) {
    if (isAtEnd()) {
      Error(ERROR_UNEXPECTED_EOF, {fileId, begin, current_index - begin}, "").raise();
    }

    consume();
  }

  auto size = current_index - begin;

  result.push_back({
    TOKEN_NUMBER,
    text.substr(begin, size),
    {fileId, begin, size}
  });
}

Lexer::Lexer(FileId fileId) : fileId(fileId), text(FileManager::get(fileId)->contents) {}

std::vector<Token> Lexer::tokenize() {
  while (!isAtEnd()) {
    skipWhitespace();

    if (isAtEnd()) {
      break;
    }

    auto [token_type, token_size] = s_token_patterns.find(text, current_index);

    bool identifier = isalnumstr(s_token_type_value_map.at(token_type)) && isIdentifierChar(text[current_index + token_size]);

    if (token_type != TOKEN_EOF && !identifier) {
      result.push_back({token_type, "", {fileId, current_index, token_size}});
      current_index += token_size;
      continue;
    }

    if (isalpha(current()) || check('_')) {
      tokenizeIdentifier();
    } else if (check('"')) {
      tokenizeString();
    } else if (check('\'')) {
      tokenizeChar();
    } else if (isdigit(current())) {
      // TODO: add hex, floating point, octal, bin, etc
      tokenizeNumber();
    }
  }

  return result;
}

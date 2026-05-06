#include "xcc/parser.h"
#include "xcc/util/fs.h"
#include "xcc/util/log.h"
#include "xcc/util/filemng.h"
#include "xcc/exceptions.h"

using namespace xcc;

static auto logger = xcc::util::log::Logger("PARSER");
static std::unordered_map<std::string, Parser::IncludedModule> module_cache;

std::shared_ptr<ast::MemberAccess> Parser::MemberAccessContext::from(const MemberAccessContext& a, const MemberAccessContext& b) {
  auto span = a.node->span + b.node->span;

  return a.pointer || b.pointer
      ? ast::MemberAccess::createByPointer(span, a.node, std::dynamic_pointer_cast<ast::Identifier>(b.node))
      : ast::MemberAccess::createByValue(span, a.node, std::dynamic_pointer_cast<ast::Identifier>(b.node));
}

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
    throw std::runtime_error("Tried to get previous token, while current is at index 0");
  }

  return tokens[current_idx-1];
}

Token Parser::next() {
  if (current_idx + 1 >= tokens.size()) {
    throw std::runtime_error("Tried to get next token, which doesn't exist");
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

bool Parser::checkNext(TokenType expected) {
  return next().type == expected;
}

std::shared_ptr<ast::Identifier> Parser::parseIdentifier(const std::string& ex_msg) {
  if (checkAdvance(TOKEN_SELF)) {
    return ast::Identifier::create(previous().span, "self");
  }

  if (!checkAdvance(TOKEN_IDENTIFIER)) {
    Error(ERROR_MISSING_IDENTIFIER, current().span, "Expected identifier " + ex_msg).raise();
  }

  return ast::Identifier::create(previous().span, previous().value);
}

std::shared_ptr<ast::Identifier> Parser::parseIdentifierWithCurrentScope(const std::string& ex_msg) {
  auto id = parseIdentifier(ex_msg);
  id->scope = module.stack;
  return id;
}

std::shared_ptr<ast::Identifier> Parser::parseScopedIdentifier(const std::string& ex_msg) {
  auto first = parseIdentifier(ex_msg);

  if (!checkAdvance(TOKEN_SCOPE)) {
    return first;
  }

  auto span = first->span;
  std::vector<std::string> nodes = {first->value};

  do {
    if (!check(TOKEN_IDENTIFIER)) {
      break;
    }

    nodes.push_back(parseIdentifier("for member access")->value);
    span += previous().span;
  } while (checkAdvance(TOKEN_SCOPE));

  /* Very specific error, shouldn't happen */
  if (nodes.empty()) {
    Error(ERROR_INVALID_MEMBER_ACCESS, current().span, "there are no access nodes").raise();
  }

  auto name = nodes.back();

  nodes.pop_back();

  auto id = ast::Identifier::create(span, name, nodes);

  return id;
}

std::shared_ptr<ast::Node> Parser::parseType() {
  auto span = current().span;

  if (checkAdvance(TOKEN_FN)) {
    if (!checkAdvance(TOKEN_LEFT_PAREN)) {
      Error(ERROR_FN_TYPE_MISSING_OPENING_PAREN, current().span, "").raise();
    }

    std::vector<std::shared_ptr<ast::Node>> args;
    bool isVariadic = false;

    do {
      if (checkAdvance(TOKEN_3_DOTS)) {
        isVariadic = true;
        break;
      }

      args.push_back(parseType());
    } while (checkAdvance(TOKEN_COMMA));

    if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
      Error(ERROR_FN_TYPE_MISSING_CLOSING_PAREN, current().span, "").raise();
    }

    if (!checkAdvance(TOKEN_RIGHT_ARROW)) {
      Error(ERROR_FN_TYPE_MISSING_ARROW, current().span, "").raise();
    }

    std::shared_ptr<ast::Node> returnType = parseType();

    return ast::Type::createFunction(span + previous().span, returnType, args, isVariadic);
  }

  auto id = parseScopedIdentifier("for type name");

  if (check(TOKEN_NOT) && checkNext(TOKEN_LEFT_PAREN)) {
    return parseCall(id);
  }

  // Check if referenced type was declared inside of this module
  auto isDeclaredInModule = std::find(module.typeAliases.begin(), module.typeAliases.end(), id->value) != module.typeAliases.end();

  // If currently parsing a module, referenced type was declared in this module, and no scope is present
  if (isModule && isDeclaredInModule && id->scope.empty()) {
    // Set type's scope to current module stack
    id->scope = module.stack;
  }

  std::shared_ptr<ast::Type> type;

  while (checkAdvance(TOKEN_STAR)) {
    type = type
      ? ast::Type::create(span + previous().span, type, true)
      : ast::Type::create(span + previous().span, ast::Node::cast(id), true);
  }

  if (!type) {
    type = ast::Type::create(span, ast::Node::cast(id), false);
  }

  return type;
}

std::shared_ptr<ast::TypedIdentifier> Parser::parseValueDecl() {
  auto span = current().span;

  std::shared_ptr<ast::Identifier> name = parseIdentifier("for variable name");
  std::shared_ptr<ast::Node> type;
  std::shared_ptr<ast::Node> value;

  if (checkAdvance(TOKEN_COLON)) {
    type = parseType();
  }

  if (checkAdvance(TOKEN_EQUALS)) {
    value = parseExpr();
  }

  return ast::TypedIdentifier::create(span + previous().span, name, type, value);
}

std::shared_ptr<ast::Node> Parser::parseFunction(bool isMethod) {
  bool is_extern = false;
  bool is_variadic = false;

  auto span = current().span;

  if (check(TOKEN_EXTERN)) {
    advance();
    is_extern = true;
  }

  if (!checkAdvance(TOKEN_FN)) {
    Error(ERROR_FN_MISSING_KEYWORD, current().span, "").raise();
  }

  auto name = parseIdentifierWithCurrentScope("for function name");

  if (isMethod) {
    name->value = structStack.back() + "_" + name->value;
  }

  std::vector<std::shared_ptr<ast::TypedIdentifier>> args;
  std::shared_ptr<ast::Node> return_type;

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    Error(ERROR_FN_MISSING_OPENING_PAREN, current().span, "").raise();
  }

  if (isMethod && checkAdvance(TOKEN_SELF)) {
    args.push_back(ast::TypedIdentifier::create(
      span + current().span,
      ast::Identifier::create(previous().span, "self"),
        ast::Type::create(previous().span, ast::Identifier::create(previous().span, structStack.back(), module.stack), true)
      )
    );

    checkAdvance(TOKEN_COMMA);
  }

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      if (checkAdvance(TOKEN_3_DOTS)) {
        is_variadic = true;
        break;
      }

      args.push_back(parseValueDecl());
    } while (checkAdvance(TOKEN_COMMA));
  }

  if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
    Error(ERROR_FN_MISSING_CLOSING_PAREN, current().span, "").raise();
  }

  if (checkAdvance(TOKEN_RIGHT_ARROW)) {
    return_type = parseType();
  } else {
    return_type = ast::Type::create(previous().span, ast::Identifier::create(previous().span, "void"), false);;
  }

  auto fndecl = ast::FnDecl::create(span + previous().span, name, return_type, args, is_extern, is_variadic);

  if (!check(TOKEN_LEFT_BRACE)) {
    if (!checkAdvance(TOKEN_SEMICOLON)) {
      Error(ERROR_FN_MISSING_SEMICOLON, previous().span, "")
        .note({}, "Function signature must be followed either by a body, or semicolon - if it's a forward-declaration")
        .raise();
    }

    // Workaround: erase scope, if it's an extern forward declaration
    // TODO: Won't work for something like `fn a::b::c();`, but then again, is it needed?
    if (fndecl->isExtern) {
      fndecl->name->scope = {};
    }

    return ast::Node::cast(fndecl);
  }

  auto body = parseBlock();

  return ast::FnDef::create(span + previous().span, fndecl, body);
}

std::shared_ptr<ast::Block> Parser::parseBlock(bool parseTopLevel) {
  auto span = current().span;

  if (!checkAdvance(TOKEN_LEFT_BRACE)) {
    Error(ERROR_BLOCK_MISSING_OPENING_BRACE, current().span, "").raise();
  }

  std::vector<std::shared_ptr<ast::Node>> nodes;

  bool shouldContinue = true;

  do {
    if (isAtEnd() || check(TOKEN_RIGHT_BRACE)) {
      break;
    }
    nodes.push_back(parseStmt(parseTopLevel));
    shouldContinue = previous().is(TOKEN_RIGHT_BRACE) || checkAdvance(TOKEN_SEMICOLON);
  } while (shouldContinue);

  if (!checkAdvance(TOKEN_RIGHT_BRACE)) {
    Error(ERROR_BLOCK_MISSING_CLOSING_BRACE, current().span, "").raise();
  }

  return ast::Block::create(span + previous().span, nodes);
}

std::shared_ptr<ast::Node> Parser::parseVar(bool global) {
  auto span = current().span;

  if (!checkAdvance(TOKEN_VAR)) {
    Error(ERROR_VAR_MISSING_KEYWORD, current().span, "").raise();
  }

  auto valdecl = parseValueDecl();

  return ast::VarDecl::create(span + previous().span, valdecl->name, valdecl->value_type, valdecl->value, global);
}

std::shared_ptr<ast::Node> Parser::parseStruct(const ast::Node::AttributeList& attrs) {
  auto span = current().span;

  if (!checkAdvance(TOKEN_STRUCT)) {
    Error(ERROR_STRUCT_MISSING_KEYWORD, current().span, "").raise();
  }

  auto name = parseIdentifierWithCurrentScope("for struct name");

  if (!checkAdvance(TOKEN_LEFT_BRACE)) {
    Error(ERROR_STRUCT_MISSING_OPENING_BRACE, current().span, "").raise();
  }

  std::vector<std::shared_ptr<ast::TypedIdentifier>> fields;
  std::vector<std::shared_ptr<ast::Node>> methods;

  // important: don't use name(), as it will prepend the same prefix as parseScopedIdentified in parseFunction
  structStack.push_back(name->value);

  bool shouldContinue = true;

  do {
    if (isAtEnd() || check(TOKEN_RIGHT_BRACE)) {
      break;
    }

    if (check(TOKEN_FN)) {
      methods.push_back(parseFunction(true));
    } else {
      fields.push_back(parseValueDecl());
    }

    shouldContinue = previous().is(TOKEN_RIGHT_BRACE) || checkAdvance(TOKEN_SEMICOLON);
  } while (shouldContinue);

  structStack.pop_back();

  if (!checkAdvance(TOKEN_RIGHT_BRACE)) {
    Error(ERROR_STRUCT_MISSING_CLOSING_BRACE, current().span, "").raise();
  }

  return ast::Struct::create(span + previous().span, name, fields, methods);
}

std::shared_ptr<ast::Node> Parser::parseIf() {
  auto span = current().span;

  if (!checkAdvance(TOKEN_IF)) {
    Error(ERROR_IF_MISSING_KEYWORD, current().span, "").raise();
  }

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    Error(ERROR_IF_MISSING_OPENING_PAREN, current().span, "").raise();
  }

  std::shared_ptr<ast::Node> cond = parseExpr();

  if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
    Error(ERROR_IF_MISSING_CLOSING_PAREN, current().span, "").raise();
  }

  std::shared_ptr<ast::Node> then_body = parseStmt();
  std::shared_ptr<ast::Node> else_body = nullptr;

  if (checkAdvance(TOKEN_ELSE)) {
    else_body = parseStmt();
  }

  return ast::If::create(span + previous().span, cond, then_body, else_body);
}

std::shared_ptr<ast::Node> Parser::parseFor() {
  auto span = current().span;

  if (!checkAdvance(TOKEN_FOR)) {
    Error(ERROR_FOR_MISSING_KEYWORD, current().span, "").raise();
  }

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    Error(ERROR_FOR_MISSING_OPENING_PAREN, current().span, "").raise();
  }

  std::shared_ptr<ast::Node> init = parseVar(false);

  if (!checkAdvance(TOKEN_SEMICOLON)) {
    Error(ERROR_FOR_MISSING_INIT_SEMICOLON, current().span, "").raise();
  }

  std::shared_ptr<ast::Node> cond = parseExpr();

  if (!checkAdvance(TOKEN_SEMICOLON)) {
    Error(ERROR_FOR_MISSING_COND_SEMICOLON, current().span, "").raise();
  }

  std::shared_ptr<ast::Node> step = parseExpr();

  if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
    Error(ERROR_FOR_MISSING_CLOSING_PAREN, current().span, "").raise();
  }

  std::shared_ptr<ast::Node> body = parseStmt();

  return ast::For::create(span + previous().span, ast::Node::cast<ast::VarDecl>(init), cond, step, body);
}

std::shared_ptr<ast::Node> Parser::parseWhile() {
  auto span = current().span;

  if (!checkAdvance(TOKEN_WHILE)) {
    Error(ERROR_WHILE_MISSING_KEYWORD, current().span, "").raise();
  }

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    Error(ERROR_WHILE_MISSING_OPENING_PAREN, current().span, "").raise();
  }

  std::shared_ptr<ast::Node> cond = parseExpr();

  if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
    Error(ERROR_WHILE_MISSING_CLOSING_PAREN, current().span, "").raise();
  }

  std::shared_ptr<ast::Node> body = parseStmt();

  return ast::While::create(span + previous().span, cond, body);
}

std::shared_ptr<ast::Node> Parser::parseReturn() {
  auto span = current().span;

  if (!checkAdvance(TOKEN_RETURN)) {
    Error(ERROR_RETURN_MISSING_KEYWORD, current().span, "").raise();
  }

  std::shared_ptr<ast::Node> expr;

  if (!check(TOKEN_SEMICOLON)) {
    expr = parseExpr();
  }

  return ast::Return::create(span + previous().span, expr);
}

std::shared_ptr<ast::Node> Parser::parseUse(const ast::Node::AttributeList& attrs) {
  bool scoped = false;

  auto span = current().span;

  if (!checkAdvance(TOKEN_USE)) {
    Error(ERROR_USE_MISSING_KEYWORD, current().span, "").raise();
  }

  if (checkAdvance(TOKEN_MOD)) {
    scoped = true;
  }

  auto name = parseIdentifier("for module name");

  if (!checkAdvance(TOKEN_SEMICOLON)) {
    Error(ERROR_USE_MISSING_SEMICOLON, current().span, "").raise();
  }

  auto path_attr = std::find_if(attrs.begin(), attrs.end(), [](auto& a) { return a.name == "path"; });
  std::string path;

  if (path_attr != attrs.end()) {
    assertRaise(path_attr->args.size() == 1, Error(ERROR_PATH_ATTR_ARG_MISMATCH, current().span, ""));
    assertRaise(path_attr->args[0]->is(ast::AST_EXPR_STRING), Error(ERROR_PATH_ATTR_ARG_BAD_TYPE, current().span, ""));

    path = path_attr->args[0]->as<ast::String>()->value;
  }

  auto res = path.empty() ? includeModule(name->name(), name->span, scoped) : includeModuleFromPath(name->name(), path, name->span, scoped);

  // Happens, if module was already included
  if (!res.body) return ast::Empty::create();

  auto mod = ast::Module::create(span + previous().span, name, res.body);

  mod->addAttribute({"__xcc_tag_used_from", { ast::String::create(span, res.path) }});

  return mod;
}

std::shared_ptr<ast::Node> Parser::parseMod(const ast::Node::AttributeList& attrs) {
  auto span = current().span;

  if (!checkAdvance(TOKEN_MOD)) {
    Error(ERROR_MOD_MISSING_KEYWORD, current().span, "").raise();
  }

  auto name = parseIdentifier("for module name");

  if (!checkAdvance(TOKEN_LEFT_BRACE)) {
    if (!checkAdvance(TOKEN_SEMICOLON)) {
      Error(ERROR_MOD_MISSING_SEMICOLON, current().span, "").raise();
    }

    if (isModule) {
      // Workaround for generated `mod` from `use` having another `mod` with the same name inside
      return ast::Empty::create();
    }

    // if (!module.stack.empty()) {
    //   throw ParserException(current().line, "There can be only one file-scoped module in the file");
    // }

    module.stack.push_back(name->name());
    return ast::Module::create(span + previous().span, name, ast::Block::create({}, {}));
  }

  auto body = ast::Block::create(span, {});

  module.stack.push_back(name->name());

  while (!check(TOKEN_RIGHT_BRACE) && !isAtEnd()) {
    body->body.push_back(parseOneTopLevelNode(false, {}));
  }

  module.stack.pop_back();

  if (!checkAdvance(TOKEN_RIGHT_BRACE)) {
    Error(ERROR_MOD_MISSING_CLOSING_BRACE, current().span, "").raise();
  }

  body->span = span + previous().span;

  return ast::Module::create(span + previous().span, name, body);
}

std::shared_ptr<ast::Node> Parser::parseTypeDeclaration(const ast::Node::AttributeList& attrs) {
  auto span = current().span;

  if (!checkAdvance(TOKEN_TYPE)) {
    Error(ERROR_TYPE_MISSING_KEYWORD, current().span, "").raise();
  }

  auto name = parseIdentifierWithCurrentScope("for type alias name");

  if (!checkAdvance(TOKEN_EQUALS)) {
    Error(ERROR_TYPE_MISSING_EQUALS, current().span, "").raise();
  }

  auto type = parseType();

  if (!checkAdvance(TOKEN_SEMICOLON)) {
    Error(ERROR_TYPE_MISSING_SEMICOLON, current().span, "").raise();
  }

  if (isModule) {
    module.typeAliases.push_back(name->value);
  }

  return ast::TypeDecl::create(span + previous().span, name, type);
}

std::shared_ptr<ast::Node> Parser::parseMacro(const ast::Node::AttributeList& attrs) {
  auto span = current().span;

  if (!checkAdvance(TOKEN_MACRO)) {
    Error(ERROR_MACRO_MISSING_KEYWORD, current().span, "").raise();
  }

  auto id = parseIdentifierWithCurrentScope("for macro name");

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    Error(ERROR_MACRO_MISSING_OPENING_PAREN, current().span, "").raise();
  }

  std::vector<std::shared_ptr<ast::Identifier>> args;

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      if (isAtEnd() || check(TOKEN_RIGHT_PAREN)) {
        break;
      }

      args.push_back(parseIdentifier("for macro argument"));
    } while (checkAdvance(TOKEN_COMMA));
  }

  if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
    Error(ERROR_MACRO_MISSING_CLOSING_PAREN, current().span, "").raise();
  }

  auto body = parseBlock(true);

  return ast::Macro::create(span + previous().span, id, args, body);
}

std::shared_ptr<ast::Node> Parser::parseStmt(bool parseTopLevel) {
  switch (current().type) {
    case TOKEN_VAR:         return parseVar(false);
    case TOKEN_IF:          return parseIf();
    case TOKEN_FOR:         return parseFor();
    case TOKEN_WHILE:       return parseWhile();
    case TOKEN_RETURN:      return parseReturn();
    case TOKEN_EXTERN:
    case TOKEN_FN: {
      assertRaise(parseTopLevel, Error(ERROR_INVALID_TOKEN_FOR_CONTEXT, current().span, "Unexpected 'fn' in current context"));
      return parseFunction(false);
    }
    case TOKEN_STRUCT: {
      assertRaise(parseTopLevel, Error(ERROR_INVALID_TOKEN_FOR_CONTEXT, current().span, "Unexpected 'struct' in current context"));
      return parseStruct({});
    }
    default:                return parseExpr();
  }
}

std::shared_ptr<ast::Node> Parser::parseExpr() {
  if (check(TOKEN_LEFT_BRACE)) {
    return parseBlock();
  }

  return parseAssignment();
}

std::shared_ptr<ast::Node> Parser::parseAssignment() {
  auto expr = parseLogicAndBitOps();

  while (checkAdvanceAnyOf(TOKEN_EQUALS, TOKEN_ADD_EQUALS, TOKEN_MIN_EQUALS, TOKEN_MUL_EQUALS, TOKEN_DIV_EQUALS,
                           TOKEN_AND_EQUALS, TOKEN_OR_EQUALS, TOKEN_LOGICAL_AND_EQUALS, TOKEN_LOGICAL_OR_EQUALS)) {
    Token op = previous();
    auto rhs = parseLogicAndBitOps();
    if (expr->isAnyOf(ast::AST_EXPR_IDENTIFIER, ast::AST_EXPR_UNARY, ast::AST_EXPR_SUBSCRIPT, ast::AST_EXPR_MEMBER_ACCESS)) {
      expr = ast::Assign::create(expr->span + rhs->span, op, expr, rhs);
    } else {
      Error(ERROR_INVALID_LHS_FOR_ASSIGNMENT, expr->span, "{} is not valid for LHS in assignment", ast::Node::typeToHumanReadableString(expr->type)).raise();
    }
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseLogicAndBitOps() {
  auto expr = parseEquality();

  while (checkAdvanceAnyOf(TOKEN_AND, TOKEN_OR, TOKEN_AMP, TOKEN_VERTICAL_LINE)) {
    Token op = previous();
    auto rhs = parseEquality();
    expr = ast::Binary::create(expr->span + rhs->span, op, expr, rhs);
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseEquality() {
  auto expr = parseComparison();

  while (checkAdvanceAnyOf(TOKEN_EQUALS_EQUALS, TOKEN_NOT_EQUALS)) {
    Token op = previous();
    auto rhs = parseComparison();
    expr = ast::Binary::create(expr->span + rhs->span, op, expr, rhs);
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseComparison() {
  auto expr = parseTerm();

  while (checkAdvanceAnyOf(TOKEN_GREATER, TOKEN_GREATER_EQUALS,
                               TOKEN_LESS, TOKEN_LESS_EQUALS)) {
    Token op = previous();
    auto rhs = parseTerm();
    expr = ast::Binary::create(expr->span + rhs->span, op, expr, rhs);
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseTerm() {
  auto expr = parseFactor();

  while (checkAdvanceAnyOf(TOKEN_MINUS, TOKEN_PLUS)) {
    Token op = previous();
    auto rhs = parseFactor();
    expr = ast::Binary::create(expr->span + rhs->span, op, expr, rhs);
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseFactor() {
  auto expr = parseCast();

  // TODO: %
  while (checkAdvanceAnyOf(TOKEN_SLASH, TOKEN_STAR)) {
    Token op = previous();
    auto rhs = parseCast();
    expr = ast::Binary::create(expr->span + rhs->span, op, expr, rhs);
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseCast() {
  auto expr = parseUnary();

  if (checkAdvance(TOKEN_AS)) {
    auto type = parseType();
    return ast::Cast::create(expr->span + type->span, expr, type);
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseUnary() {
  if (checkAdvanceAnyOf(TOKEN_NOT, TOKEN_MINUS, TOKEN_TILDA,
                        TOKEN_AMP, TOKEN_STAR)) {
    Token op = previous();
    auto rhs = parseUnary();
    return ast::Unary::create(op.span + rhs->span, op, rhs);
  }

  return parseSubscript();
}

std::shared_ptr<ast::Node> Parser::parseSubscript() {
  auto lhs = parseRvalue();

  if (checkAdvance(TOKEN_LEFT_SQUARE_BRACE)) {
    auto rhs = parseExpr();
    assertRaise(checkAdvance(TOKEN_RIGHT_SQUARE_BRACE), Error(ERROR_SUBSCRIPT_MISSING_CLOSING_BRACE, rhs->span.pointPastLast(), ""));
    return ast::Subscript::create(lhs->span + rhs->span, lhs, rhs);
  }

  return lhs;
}

std::shared_ptr<ast::Node> Parser::parseNumber() {
  if (previous().value.find('.') != std::string::npos) {
    return ast::Number::createFloating(previous().span, std::stod(previous().value));
  }

  std::string value = previous().value;

  auto res = util::determineBase(value);

  return ast::Number::createInteger(previous().span, std::stol(res.value, nullptr, res.base));
}

std::shared_ptr<ast::Node> Parser::parseRvalue() {
  if (checkAdvance(TOKEN_NUMBER)) {
    return parseNumber();
  }

  if (checkAdvance(TOKEN_STRING)) {
    return ast::String::create(previous().span, previous().value);
  }

  if (checkAdvance(TOKEN_CHAR)) {
    return ast::Number::createInteger(previous().span, previous().value[0]);
  }

  if (checkAdvance(TOKEN_DOLLAR_SIGN)) {
    auto span = previous().span;

    if (!checkAdvance(TOKEN_IDENTIFIER)) {
      Error(ERROR_DOLLAR_MISSING_IDENTIFIER, previous().span.pointPastLast(), "").raise();
    }

    char * value = getenv(previous().value.c_str());

    if (!value) {
      Error(ERROR_NO_ENV_VARIABLE, previous().span, "'{}'", previous().value).raise();
    }

    return ast::String::create(span + previous().span, value);
  }

  if (checkAdvance(TOKEN_LEFT_PAREN)) {
    auto expr = parseExpr();
    if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
      Error(ERROR_EXPR_MISSING_CLOSING_PAREN, previous().span.pointPastLast(), "").raise();
    }
    return expr;
  }

  return parseLvalueAndCall();
}

std::shared_ptr<ast::Node> Parser::parseLvalueAndCall() {
  auto span = current().span;

  if (!check(TOKEN_IDENTIFIER) && !check(TOKEN_SELF)) {
    Error(ERROR_LVALUE_UNEXPECTED_TOKEN, current().span, "'{}' ({})", current().value, Token::typeToString(current().type)).raise();
  }

  /* Parse Member Access */
  if (checkNext(TOKEN_DOT) || checkNext(TOKEN_RIGHT_ARROW)) {
    std::vector<MemberAccessContext> nodes;

    do {
      if (!checkAnyOf(TOKEN_IDENTIFIER, TOKEN_SELF)) {
        break;
      }

      nodes.push_back({parseIdentifier("for member access"), current().is(TOKEN_RIGHT_ARROW)});
    } while (checkAdvance(TOKEN_DOT) || checkAdvance(TOKEN_RIGHT_ARROW));

    /* Very specific error, shouldn't happen */
    assertRaise(!nodes.empty(), Error(ERROR_INVALID_MEMBER_ACCESS, span + previous().span, ""));

    /* If do-while loop finished without triggering ParserException("Expected identifier ...")
     * it is guaranteed that at least 2 elements will be present in nodes */
    std::shared_ptr<ast::MemberAccess> memberAccess = MemberAccessContext::from(nodes[0], nodes[1]);;

    /* Recursively reduce the rest of nodes into single MemberAccess tree. If only 2 nodes are present -
     * won't get executed */
    for (size_t i = 2; i < nodes.size(); ++i) {
      memberAccess = MemberAccessContext::from({memberAccess, false}, nodes[i]);
    }

    /* Method Call */
    if (check(TOKEN_LEFT_PAREN)) {
      return parseCall(memberAccess);
    }

    return memberAccess;
  }

  auto id = parseScopedIdentifier("for function name (or not?)");

  if (check(TOKEN_LEFT_PAREN) || (check(TOKEN_NOT) && checkNext(TOKEN_LEFT_PAREN))) {
    /* Function or Macro Call */
    return parseCall(id);
  }

  return ast::Node::cast<ast::Node>(id);
}

std::shared_ptr<ast::Node> Parser::parseCall(std::shared_ptr<ast::Node> callee) {
  std::vector<std::shared_ptr<ast::Node>> args;

  bool isMacro = false;

  if (checkAdvance(TOKEN_NOT)) {
    assertRaise(callee->is(ast::AST_EXPR_IDENTIFIER),
      Error(ERROR_MACRO_CALLEE_IS_NOT_ID, callee->span, ""));
    isMacro = true;
  }

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    Error(isMacro ? ERROR_MACRO_CALL_MISSING_OPENING_PAREN : ERROR_FN_CALL_MISSING_OPENING_PAREN, current().span, "").raise();
  }

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      if (isAtEnd() || check(TOKEN_RIGHT_PAREN)) {
        break;
      }

      /* Parse *any* node for macros arg. Use isRepl=true for this, which allows for
       * parsing exprs from top-level context */
      args.push_back(isMacro ? parseOneTopLevelNode(true, {}) : parseExpr());
    } while (checkAdvance(TOKEN_COMMA));
  }

  if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
    Error(isMacro ? ERROR_MACRO_CALL_MISSING_CLOSING_PAREN : ERROR_FN_CALL_MISSING_CLOSING_PAREN, previous().span.pointPastLast(), "").raise();
  }

  if (isMacro) {
    return ast::MacroCall::create(callee->span + previous().span, ast::Node::cast<ast::Identifier>(callee), args);
  }

  return ast::Call::create(callee->span + previous().span, callee, args);
}

ast::Node::AttributeList Parser::parseAttributeList() {
  if (!checkAdvance(TOKEN_LEFT_SQUARE_BRACE)) {
    Error(ERROR_ATTR_MISSING_OPENING_BRACKET, current().span, "").raise();
  }

  ast::Node::AttributeList attrs;

  do {
    if (isAtEnd() || check(TOKEN_RIGHT_SQUARE_BRACE)) {
      break;
    }

    auto span = current().span;

    auto name = parseIdentifier("for attribute name");

    std::vector<std::shared_ptr<ast::Node>> args;

    if (checkAdvance(TOKEN_LEFT_PAREN)) {
      do {
        if (isAtEnd() || check(TOKEN_RIGHT_PAREN) || check(TOKEN_RIGHT_SQUARE_BRACE)) {
          break;
        }

        args.push_back(parseExpr());
      } while (checkAdvance(TOKEN_COMMA));

      if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
        Error(ERROR_ATTR_MISSING_CLOSING_PAREN, previous().span.pointPastLast(), "").raise();
      }
    }

    attrs.push_back({name->name(), args, span + previous().span});
  } while (checkAdvance(TOKEN_COMMA));

  if (!checkAdvance(TOKEN_RIGHT_SQUARE_BRACE)) {
    Error(ERROR_ATTR_MISSING_CLOSING_BRACKET, previous().span.pointPastLast(), "").raise();
  }

  return attrs;
}

Parser::IncludedModule Parser::includeModule(const std::string& name, SourceSpan span, bool scoped) {
  return includeModuleFromPath(name, resolveModulePath(name, span), span, scoped);
}

Parser::IncludedModule Parser::includeModuleFromPath(const std::string& name, const std::string& path, SourceSpan span, bool scoped) {
  IncludedModule result;

  if (std::find(module.included.begin(), module.included.end(), name) != module.included.end()) {
    logger.warn("Skipping inclusion of '{}', as it was already included", name);
    return {};
  }

  if (module_cache.contains(name)) {
    logger.info("Using cached module '{}'", name);
    module.included.emplace(name);
    return module_cache[name];
  }

  module.included.emplace(name);

  result.path = path;
  auto file = FileManager::load(result.path);

  logger.info("Found module '{}' at {}", name, result.path);

  try {
    auto lexer  = Lexer(file);
    auto tokens = lexer.tokenize();
    auto parser = Parser(file, tokens, true);

    if (scoped) {
      parser.module.stack = module.stack;
    }

    parser.module.stack.push_back(name);
    parser.module.searchPaths = module.searchPaths;
    parser.module.included    = module.included;

    auto mod = parser.parse(false);

    result.body = moduleReplaceDefinitions(mod);

    module_cache[name] = result;

    parser.module.stack.pop_back();

    // Copy list of already included modules over to current parser
    module.included.merge(parser.module.included);
  } catch (CompilationException& ex) {
    ex.error.note(span, "During inclusion of module '{}' ({})", name, path).raise();
  }

  return result;
}

std::string Parser::resolveModulePath(const std::string& name, SourceSpan span) {
  auto filename = name + ".xc";

  if (fs::exists(filename)) {
    return filename;
  }

  for (auto& searchPath : module.searchPaths) {
    auto path = searchPath + "/" + filename;

    if (fs::exists(path)) {
      return path;
    }
  }

  Error(ERROR_MODULE_NOT_FOUND, span, "Could not resolve path to module '{}'", name).raise();
}

std::shared_ptr<ast::Block> Parser::moduleReplaceDefinitions(const std::shared_ptr<ast::Block>& body) {
  auto result = ast::Block::create({}, {});

  for (auto & node : body->body) {
    if (node->isAnyOf(ast::AST_FUNCTION_DECL, ast::AST_TYPE_DECL, ast::AST_MACRO)) {
      result->body.push_back(node);
    } else if (node->is(ast::AST_MOD)) {
      auto mod = node->as<ast::Module>();
      mod->body = moduleReplaceDefinitions(mod->body);
      result->body.push_back(node);
    } else if (node->is(ast::AST_FUNCTION_DEF)) {
      result->body.push_back(node->as<ast::FnDef>()->decl);
    } else if (node->is(ast::AST_STRUCT)) {
      auto s = node->as<ast::Struct>();

      for (size_t j = 0; j < s->methods.size(); ++j) {
        s->methods[j] = s->methods[j]->as<ast::FnDef>()->decl;
      }

      result->body.push_back(node);
    }
  }

  return result;
}

std::shared_ptr<ast::Node> Parser::parseOneTopLevelNode(bool isRepl, const ast::Node::AttributeList& attrs) {
  if (check(TOKEN_LEFT_SQUARE_BRACE)) {
    auto new_attrs = parseAttributeList();
    auto node  = parseOneTopLevelNode(isRepl, new_attrs);
    node->attributes = new_attrs;
    return node;
  }

  if (checkAnyOf(TOKEN_FN, TOKEN_EXTERN)) {
    return parseFunction(false);
  }

  if (check(TOKEN_VAR)) {
    auto var = parseVar(true);
    if (!checkAdvance(TOKEN_SEMICOLON)) {
      Error(ERROR_VARDECL_MISSING_SEMICOLON, previous().span.pointPastLast(), "").raise();
    }
    return var;
  }

  if (check(TOKEN_STRUCT)) {
    return parseStruct(attrs);
  }

  if (check(TOKEN_USE)) {
    return parseUse(attrs);
  }

  if (check(TOKEN_MOD)) {
    return parseMod(attrs);
  }

  if (check(TOKEN_TYPE)) {
    return parseTypeDeclaration(attrs);
  }

  if (check(TOKEN_MACRO)) {
    return parseMacro(attrs);
  }

  if (isRepl) {
    return parseStmt();
  }

  try {
    // Allow macro calls to be parsed at top-level
    auto id = parseScopedIdentifier("for macro name (this is a last resort to parse something)");
    if (check(TOKEN_NOT) && checkNext(TOKEN_LEFT_PAREN)) {
      return parseCall(id);
    }
  } catch (CompilationException&) {
    // Ignore, there is a better worded exception at the end of this function
  }

  Error(ERROR_TOP_LEVEL_UNEXPECTED_TOKEN, current().span, "'{}' ({})", current().value, Token::typeToString(current().type)).raise();
}

Parser::Parser(FileId fileId, const std::vector<Token>& tokens, bool isModule) : fileId(fileId), tokens(tokens), current_idx(0), isModule(isModule) {}

std::shared_ptr<ast::Block> Parser::parse(bool isRepl) {
  auto block = ast::Block::create({}, {});

  while (!isAtEnd()) {
    block->body.push_back(parseOneTopLevelNode(isRepl, {}));
  }

  return block;
}

void Parser::addModuleSearchPath(const std::string& path) {
  module.searchPaths.push_back(path);
}


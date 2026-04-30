#include "xcc/parser.h"
#include "xcc/util/fs.h"
#include "xcc/util/log.h"
#include "xcc/exceptions.h"

using namespace xcc;

static auto logger = xcc::util::log::Logger("PARSER");
static std::unordered_map<std::string, Parser::IncludedModule> module_cache;

std::shared_ptr<ast::MemberAccess> Parser::MemberAccessContext::from(const MemberAccessContext& a, const MemberAccessContext& b) {
  return a.pointer || b.pointer
      ? ast::MemberAccess::createByPointer(a.node, std::dynamic_pointer_cast<ast::Identifier>(b.node))
      : ast::MemberAccess::createByValue(a.node, std::dynamic_pointer_cast<ast::Identifier>(b.node));
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

bool Parser::checkNext(TokenType expected) {
  return next().type == expected;
}

std::shared_ptr<ast::Identifier> Parser::parseIdentifier(const std::string& ex_msg) {
  if (checkAdvance(TOKEN_SELF)) {
    return ast::Identifier::create("self");
  }

  if (!checkAdvance(TOKEN_IDENTIFIER)) {
    throw ParserException(current().line, "Expected identifier " + ex_msg);
  }

  return ast::Identifier::create(previous().value);
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

  std::vector<std::string> nodes = {first->value};

  do {
    if (!check(TOKEN_IDENTIFIER)) {
      break;
    }

    nodes.push_back(parseIdentifier("for member access")->value);
  } while (checkAdvance(TOKEN_SCOPE));

  /* Very specific error, shouldn't happen */
  assertThrow(!nodes.empty(), ParserException("Invalid member scope access state (nodes.size=0)"));

  auto name = nodes.back();

  nodes.pop_back();

  auto id = ast::Identifier::create(name, nodes);

  return id;
}

std::shared_ptr<ast::Node> Parser::parseType() {
  if (checkAdvance(TOKEN_FN)) {
    if (!checkAdvance(TOKEN_LEFT_PAREN)) {
      throw ParserException(current().line, "Expected '(' after 'fn' for type");
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
      throw ParserException(current().line, "Expected ')' after function type args");
    }

    if (!checkAdvance(TOKEN_RIGHT_ARROW)) {
      throw ParserException(current().line, "Expected '->' after function type args");
    }

    std::shared_ptr<ast::Node> returnType = parseType();

    return ast::Type::createFunction(returnType, args, isVariadic);
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
    type = type ? ast::Type::create(type, true) : ast::Type::create(ast::Node::cast(id), true);
  }

  if (!type) {
    type = ast::Type::create(ast::Node::cast(id), false);
  }

  return type;
}

std::shared_ptr<ast::TypedIdentifier> Parser::parseValueDecl() {
  std::shared_ptr<ast::Identifier> name = parseIdentifier("for variable name");
  std::shared_ptr<ast::Node> type;
  std::shared_ptr<ast::Node> value;

  if (checkAdvance(TOKEN_COLON)) {
    type = parseType();
  }

  if (checkAdvance(TOKEN_EQUALS)) {
    value = parseExpr();
  }

  return ast::TypedIdentifier::create(name, type, value);
}

std::shared_ptr<ast::Node> Parser::parseFunction(bool isMethod) {
  bool is_extern = false;
  bool is_variadic = false;

  if (check(TOKEN_EXTERN)) {
    advance();
    is_extern = true;
  }

  if (!checkAdvance(TOKEN_FN)) {
    throw ParserException(current().line, "Expected 'fn'");
  }

  auto name = parseIdentifierWithCurrentScope("for function name");

  if (isMethod) {
    name->value = structStack.back() + "_" + name->value;
  }

  std::vector<std::shared_ptr<ast::TypedIdentifier>> args;
  std::shared_ptr<ast::Node> return_type;

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    throw ParserException(current().line, "Expected '(' after function name");
  }

  if (isMethod && checkAdvance(TOKEN_SELF)) {
    args.push_back(ast::TypedIdentifier::create(
    ast::Identifier::create("self"),
      ast::Type::create(ast::Identifier::create(structStack.back(), module.stack), true)
    ));

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
    throw ParserException(current().line, "Expected ')' after function arguments");
  }

  if (checkAdvance(TOKEN_RIGHT_ARROW)) {
    return_type = parseType();
  } else {
    return_type = ast::Type::create(ast::Identifier::create("void"), false);;
  }

  auto fndecl = ast::FnDecl::create(name, return_type, args, is_extern, is_variadic);

  if (!check(TOKEN_LEFT_BRACE)) {
    if (!checkAdvance(TOKEN_SEMICOLON)) {
      throw ParserException(current().line, "Expected ';' after function declaration");
    }

    // Workaround: erase scope, if it's an extern forward declaration
    // TODO: Won't work for something like `fn a::b::c();`, but then again, is it needed?
    if (fndecl->isExtern) {
      fndecl->name->scope = {};
    }

    return ast::Node::cast(fndecl);
  }

  auto body = parseBlock();

  return ast::FnDef::create(fndecl, body);
}

std::shared_ptr<ast::Block> Parser::parseBlock(bool parseTopLevel) {
  if (!checkAdvance(TOKEN_LEFT_BRACE)) {
    throw ParserException(current().line, "Expected '{'");
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

std::shared_ptr<ast::Node> Parser::parseStruct(const ast::Node::AttributeList& attrs) {
  if (!checkAdvance(TOKEN_STRUCT)) {
    throw ParserException(current().line, "Expected 'struct'");
  }

  auto name = parseIdentifierWithCurrentScope("for struct name");

  if (!checkAdvance(TOKEN_LEFT_BRACE)) {
    throw ParserException(current().line, "Expected '{' after 'struct'");
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
    throw ParserException(current().line, "Expected '}' after struct definition");
  }

  return ast::Struct::create(name, fields, methods);
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
    throw ParserException(current().line, "Expected ')' after 'while' condition");
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

std::shared_ptr<ast::Node> Parser::parseUse(const ast::Node::AttributeList& attrs) {
  bool scoped = false;

  if (!checkAdvance(TOKEN_USE)) {
    throw ParserException(current().line, "Expected 'use'");
  }

  if (checkAdvance(TOKEN_MOD)) {
    scoped = true;
  }

  auto name = parseIdentifier("for module name");

  if (!checkAdvance(TOKEN_SEMICOLON)) {
    throw ParserException(current().line, "Expected ';' after 'use' statement");
  }

  auto path_attr = std::find_if(attrs.begin(), attrs.end(), [](auto& a) { return a.name == "path"; });
  std::string path;

  if (path_attr != attrs.end()) {
    assertThrow(path_attr->args.size() == 1, ParserException("'path' attribute expects 1 argument"));
    assertThrow(path_attr->args[0]->is(ast::AST_EXPR_STRING), ParserException("'path' attribute expect a string argument"));

    path = path_attr->args[0]->as<ast::String>()->value;
  }

  auto res = path.empty() ? includeModule(name->name(), scoped) : includeModuleFromPath(name->name(), path, scoped);

  // Happens, if module was already included
  if (!res.body) return ast::Empty::create();

  auto mod = ast::Module::create(name, res.body);

  mod->addAttribute({"__xcc_tag_used_from", { ast::String::create(res.path) }});

  return mod;
}

std::shared_ptr<ast::Node> Parser::parseMod(const ast::Node::AttributeList& attrs) {
  if (!checkAdvance(TOKEN_MOD)) {
    throw ParserException(current().line, "Expected 'mod'");
  }

  auto name = parseIdentifier("for module name");

  if (!checkAdvance(TOKEN_LEFT_BRACE)) {
    if (!checkAdvance(TOKEN_SEMICOLON)) {
      throw ParserException(current().line, "Expected ';' after file-scoped module declaration");
    }

    if (isModule) {
      // Workaround for generated `mod` from `use` having another `mod` with the same name inside
      return ast::Empty::create();
    }

    // if (!module.stack.empty()) {
    //   throw ParserException(current().line, "There can be only one file-scoped module in the file");
    // }

    module.stack.push_back(name->name());
    return ast::Module::create(name, ast::Block::create({}));
  }

  auto body = ast::Block::create({});

  module.stack.push_back(name->name());

  while (!check(TOKEN_RIGHT_BRACE) && !isAtEnd()) {
    body->body.push_back(parseOneTopLevelNode(false, {}));
  }

  module.stack.pop_back();

  if (!checkAdvance(TOKEN_RIGHT_BRACE)) {
    throw ParserException(current().line, "Expected '}' after mod body");
  }

  return ast::Module::create(name, body);
}

std::shared_ptr<ast::Node> Parser::parseTypeDeclaration(const ast::Node::AttributeList& attrs) {
  if (!checkAdvance(TOKEN_TYPE)) {
    throw ParserException(current().line, "Expected 'type'");
  }

  auto name = parseIdentifierWithCurrentScope("for type alias name");

  if (!checkAdvance(TOKEN_EQUALS)) {
    throw ParserException(current().line, "Expected '=' after type name");
  }

  auto type = parseType();

  if (!checkAdvance(TOKEN_SEMICOLON)) {
    throw ParserException(current().line, "Expected ';' after type declaration (alias)");
  }

  if (isModule) {
    module.typeAliases.push_back(name->value);
  }

  return ast::TypeDecl::create(name, type);
}

std::shared_ptr<ast::Node> Parser::parseMacro(const ast::Node::AttributeList& attrs) {
  if (!checkAdvance(TOKEN_MACRO)) {
    throw ParserException(current().line, "Expected 'macro'");
  }

  auto id = parseIdentifierWithCurrentScope("for macro name");

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    throw ParserException(current().line, "Expected '(' after macro name");
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
    throw ParserException(current().line, "Expected ')' after macro arguments (macro definition)");
  }

  auto body = parseBlock(true);

  return ast::Macro::create(id, args, body);
}

std::shared_ptr<ast::Node> Parser::parseStmt(bool parseTopLevel) {
  switch (current().type) {
    case TOKEN_VAR:         return parseVar(false);
    case TOKEN_IF:          return parseIf();
    case TOKEN_FOR:         return parseFor();
    case TOKEN_WHILE:       return parseWhile();
    case TOKEN_RETURN:      return parseReturn();
    case TOKEN_EXTERN:
    case TOKEN_FN:          return parseTopLevel ? parseFunction(false) : throw CodegenException("Unexpected 'fn' in current context");
    case TOKEN_STRUCT:      return parseTopLevel ? parseStruct({})      : throw CodegenException("Unexpected 'struct' in current context");
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
    return ast::Unary::create(op, parseUnary());
  }

  return parseSubscript();
}

std::shared_ptr<ast::Node> Parser::parseSubscript() {
  auto lhs = parseRvalue();

  if (checkAdvance(TOKEN_LEFT_SQUARE_BRACE)) {
    auto rhs = parseExpr();
    assertThrow(checkAdvance(TOKEN_RIGHT_SQUARE_BRACE), ParserException("Missing closing ']' after '[' in subscript operator"));
    return ast::Subscript::create(lhs, rhs);
  }

  return lhs;
}

std::shared_ptr<ast::Node> Parser::parseNumber() {
  if (previous().value.find('.') != std::string::npos) {
    return ast::Number::createFloating(std::stod(previous().value));
  }

  std::string value = previous().value;

  auto res = util::determineBase(value);

  return ast::Number::createInteger(std::stol(res.value, nullptr, res.base));
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

  if (checkAdvance(TOKEN_DOLLAR_SIGN)) {
    if (!checkAdvance(TOKEN_IDENTIFIER)) {
      throw ParserException(current().line, "Expected identifier after '$'");
    }

    char * value = getenv(previous().value.c_str());

    if (!value) {
      throw ParserException(current().line, "No such env variable '" + previous().value + "'");
    }

    return ast::String::create(value);
  }

  if (checkAdvance(TOKEN_LEFT_PAREN)) {
    auto expr = parseExpr();
    if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
      throw ParserException(current().line, "Expected ')' after expression");
    }
    return expr;
  }

  return parseLvalueAndCall();
}

std::shared_ptr<ast::Node> Parser::parseLvalueAndCall() {
  if (!check(TOKEN_IDENTIFIER) && !check(TOKEN_SELF)) {
    throw ParserException(current().line, "Unexpected token '" + current().value + "'(" + Token::typeToString(current().type) + "), expected identifier or self");
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
    assertThrow(!nodes.empty(), ParserException("Invalid member access state (nodes.size=0)"));

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
    assertThrow(callee->is(ast::AST_EXPR_IDENTIFIER),
      ParserException("macro callee can only be an identifier"));
    isMacro = true;
  }

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    throw ParserException(current().line,
      std::format("Expected '(' after {} name (call)", isMacro ? "macro" : "function"));
  }

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      if (isAtEnd() || check(TOKEN_RIGHT_PAREN)) {
        break;
      }

      // Parse *any* node for macros arg. Use isRepl=true for this, which allows for
      // parsing exprs from top-level context
      args.push_back(isMacro ? parseOneTopLevelNode(true, {}) : parseExpr());
    } while (checkAdvance(TOKEN_COMMA));
  }

  if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
    throw ParserException(current().line,
      std::format("Expected ')' after {} arguments (call)", isMacro ? "macro" : "function"));
  }


  if (isMacro) {
    return ast::MacroCall::create(ast::Node::cast<ast::Identifier>(callee), args);
  }

  return ast::Call::create(callee, args);
}

ast::Node::AttributeList Parser::parseAttributeList() {
  if (!checkAdvance(TOKEN_LEFT_SQUARE_BRACE)) {
    throw ParserException(current().line, "Expected '[' at the beginning of attribute list");
  }

  ast::Node::AttributeList attrs;

  do {
    if (isAtEnd() || check(TOKEN_RIGHT_SQUARE_BRACE)) {
      break;
    }

    auto name = parseIdentifier("for attribute name");

    std::vector<std::shared_ptr<ast::Node>> args;

    if (checkAdvance(TOKEN_LEFT_PAREN)) {
      do {
        if (isAtEnd() || check(TOKEN_RIGHT_PAREN)) {
          break;
        }

        args.push_back(parseExpr());
      } while (checkAdvance(TOKEN_COMMA));

      if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
        throw ParserException(current().line, "Expected ')' after attribute");
      }
    }

    attrs.push_back({name->name(), args});
  } while (checkAdvance(TOKEN_COMMA));

  if (!checkAdvance(TOKEN_RIGHT_SQUARE_BRACE)) {
    throw ParserException(current().line, "Expected ']' after attribute list");
  }

  return attrs;
}

Parser::IncludedModule Parser::includeModule(const std::string& name, bool scoped) {
  return includeModuleFromPath(name, resolveModulePath(name), scoped);
}

Parser::IncludedModule Parser::includeModuleFromPath(const std::string& name, const std::string& path, bool scoped) {
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
  auto src = fs::readFile(result.path);

  logger.info("Found module '{}' at {}", name, result.path);

  auto lexer  = Lexer(src);
  auto tokens = lexer.tokenize();
  auto parser = Parser(tokens, true);

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

  return result;
}

std::string Parser::resolveModulePath(const std::string& name) {
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

  logger.error("Could not resolve module '{}'", name);

  throw std::runtime_error("Could not resolve module");
}

std::shared_ptr<ast::Block> Parser::moduleReplaceDefinitions(const std::shared_ptr<ast::Block>& body) {
  auto result = ast::Block::create({});

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
      throw ParserException(current().line, "Expected ';' variable declaration (global scope)");
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
  } catch (ParserException&) {
    // Ignore, there is a better worded exception at the end of this function
  }

  throw ParserException(current().line, "Unexpected token at top-level scope: '" + current().value + "' (" + Token::typeToString(current().type) + ")");
}

Parser::Parser(const std::vector<Token>& tokens, bool isModule) : tokens(tokens), current_idx(0), isModule(isModule) {}

std::shared_ptr<ast::Block> Parser::parse(bool isRepl) {
  auto block = ast::Block::create({});

  while (!isAtEnd()) {
    block->body.push_back(parseOneTopLevelNode(isRepl, {}));
  }

  return block;
}

void Parser::addModuleSearchPath(const std::string& path) {
  module.searchPaths.push_back(path);
}


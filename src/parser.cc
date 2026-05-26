#include "xcc/parser.h"
#include "xcc/codegen.h"
#include "xcc/util/fs.h"
#include "xcc/util/log.h"
#include "xcc/util/filemng.h"
#include "xcc/exceptions.h"

#include <filesystem>

#include "xcc/util/util.h"

using namespace xcc;

static auto& logger = xcc::log::Logger::get("PARSER");

std::unordered_map<std::string, IncludedModule> ModuleCache::modules;

void ModuleCache::set(const std::string& path, IncludedModule module) {
  modules[path] = std::move(module);
}

IncludedModule& ModuleCache::get(const std::string& path) {
  return modules[path];
}

bool ModuleCache::contains(const std::string& path) {
  return modules.contains(path);
}

void ModuleCache::updateDebugInfo(codegen::GlobalContext& ctx) {
  for (auto& [name, mod] : modules) {
    if (!mod.di_file) {
      mod.di_file = ctx.di_builder->createFile(fs::path::getFileName(name), fs::path::getParent(name));
    }
  }
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
    return ast::Identifier::create(previous().span, lexicalScope, "self");
  }

  if (!checkAdvance(TOKEN_IDENTIFIER)) {
    Error(ERROR_MISSING_IDENTIFIER, current().span, "Expected identifier " + ex_msg).raise();
  }

  return ast::Identifier::create(previous().span, lexicalScope, previous().value);
}

std::shared_ptr<ast::Identifier> Parser::parseScopedIdentifier(const std::string& ex_msg, bool allowGenerics) {
  auto first = parseIdentifier(ex_msg);
  auto span = first->span;

  std::vector<std::string> nodes = {first->value};
  ast::NodeList genericArgs;

  while (true) {
    // Standard generics (identifier < generic list >), which are explicitly allowed only in type scope
    if (allowGenerics && genericArgs.empty() && check(TOKEN_LESS)) {
      advance();

      do {
        genericArgs.push_back(parseType());
      } while (checkAdvance(TOKEN_COMMA));

      if (!checkAdvance(TOKEN_GREATER)) {
        Error(ERROR_GENERIC_TYPE_DECL_MISSING_CLOSING_GT, previous().span.pointPastLast()).raise();
      }
      span += previous().span;
    }

    if (checkAdvance(TOKEN_SCOPE)) {
      // Parse turbofish (::<), which can be placed anywhere
      if (check(TOKEN_LESS)) {
        advance();

        do {
          genericArgs.push_back(parseType());
        } while (checkAdvance(TOKEN_COMMA));

        if (!checkAdvance(TOKEN_GREATER)) {
          Error(ERROR_GENERIC_TYPE_DECL_MISSING_CLOSING_GT, previous().span.pointPastLast()).raise();
        }

        span += previous().span;
        break;
      }

      // Normal scope access
      nodes.push_back(parseIdentifier("for member access")->value);
      span += previous().span;
    } else {
      break;
    }
  }

  auto name = nodes.back();
  nodes.pop_back();

  return ast::Identifier::create(span, lexicalScope, name, nodes, genericArgs);
}

std::shared_ptr<ast::Node> Parser::parseType(std::shared_ptr<ast::Identifier> name) {
  std::shared_ptr<ast::Type> type;

  auto span = current().span;

  if (!name && checkAdvance(TOKEN_LEFT_SQUARE_BRACE)) {
    ast::NodeList members;

    do {
      members.push_back(parseType());
    } while (checkAdvance(TOKEN_COMMA));

    if (!checkAdvance(TOKEN_RIGHT_SQUARE_BRACE)) {
      Error(ERROR_FN_TYPE_MISSING_CLOSING_PAREN, current().span).raise();
    }

    return ast::Type::createTuple(span + previous().span, lexicalScope, members);
  }

  if (!name && checkAdvance(TOKEN_FN)) {
    ast::NodeList captures;

    if (checkAdvance(TOKEN_LEFT_SQUARE_BRACE)) {
      do {
        captures.push_back(parseType());
      } while (checkAdvance(TOKEN_COMMA));

      if (!checkAdvance(TOKEN_RIGHT_SQUARE_BRACE)) {
        Error(ERROR_LAMBDA_TYPE_MISSING_CLOSING_SQUARE_BRACE, current().span).raise();
      }
    }

    if (!checkAdvance(TOKEN_LEFT_PAREN)) {
      Error(ERROR_FN_TYPE_MISSING_OPENING_PAREN, current().span).raise();
    }

    ast::NodeList args;
    bool isVariadic = false;

    do {
      if (checkAdvance(TOKEN_3_DOTS)) {
        isVariadic = true;
        break;
      }

      args.push_back(parseType());
    } while (checkAdvance(TOKEN_COMMA));

    if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
      Error(ERROR_FN_TYPE_MISSING_CLOSING_PAREN, current().span).raise();
    }

    if (!checkAdvance(TOKEN_RIGHT_ARROW)) {
      Error(ERROR_FN_TYPE_MISSING_ARROW, current().span).raise();
    }

    std::shared_ptr<ast::Node> returnType = parseType();

    return captures.empty()
      ? ast::Type::createFunction(span + previous().span, lexicalScope, returnType, args, isVariadic)
      : ast::Type::createLambda(span + previous().span, lexicalScope, captures, returnType, args, isVariadic);
  }

  auto id = name ? name : parseScopedIdentifier("for type name", true);

  // TODO: Don't forget to pass genericArgs to parseCall when implementing generic functions
  // TODO: Should calls be allowed here? Looks off...
  if (check(TOKEN_NOT) && checkNext(TOKEN_LEFT_PAREN)) {
    return parseCall(id);
  }

  // Parse nested pointer types
  // Needs to be before array bounds, because array can have a pointer base type
  while (checkAdvance(TOKEN_STAR)) {
    type = type
      ? ast::Type::createPointer(span + previous().span, lexicalScope, type)
      : ast::Type::createPointer(span + previous().span, lexicalScope, ast::Node::cast(id));
  }

  // Parse nested array bounds
  while (checkAdvance(TOKEN_LEFT_SQUARE_BRACE)) {
    // Allow 0-sized arrays
    auto size = check(TOKEN_RIGHT_SQUARE_BRACE) ? ast::Number::createInteger(previous().span, lexicalScope, 0) : parseExpr();

    if (!checkAdvance(TOKEN_RIGHT_SQUARE_BRACE)) {
      Error(ERROR_TYPE_ARRAY_NO_CLOSING_BRACE, previous().span.pointPastLast()).raise();
    }

    type = type
      ? ast::Type::createArray(span + previous().span, lexicalScope, type, ast::Node::cast(size))
      : ast::Type::createArray(span + previous().span, lexicalScope, ast::Node::cast(id), ast::Node::cast(size));
  }

  // If base type is an array, it also can be a pointer to an array
  while (checkAdvance(TOKEN_STAR)) {
    type = type
      ? ast::Type::createPointer(span + previous().span, lexicalScope, type)
      : ast::Type::createPointer(span + previous().span, lexicalScope, ast::Node::cast(id));
  }

  if (!type) {
    type = ast::Type::create(span, lexicalScope, ast::Node::cast(id));
  }

  return type;
}

std::shared_ptr<ast::TypedIdentifier> Parser::parseValueDecl(const std::string& err_msg) {
  auto span = current().span;

  std::shared_ptr<ast::Identifier> name = parseIdentifier(err_msg);
  std::shared_ptr<ast::Node> type;
  std::shared_ptr<ast::Node> value;

  if (checkAdvance(TOKEN_COLON)) {
    type = parseType();
  }

  if (checkAdvance(TOKEN_EQUALS)) {
    value = parseExpr();
  }

  return ast::TypedIdentifier::create(span + previous().span, lexicalScope, name, type, value);
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
    Error(ERROR_FN_MISSING_KEYWORD, current().span).raise();
  }

  auto name = parseIdentifier("for function name");

  std::vector<std::shared_ptr<ast::TypedIdentifier>> args;
  std::shared_ptr<ast::Node> return_type;

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    Error(ERROR_FN_MISSING_OPENING_PAREN, current().span).raise();
  }

  if (isMethod && checkAdvance(TOKEN_SELF)) {
    auto struct_name = lexicalScope.back();

    auto type_id = ast::Identifier::create(previous().span, lexicalScope, struct_name);
    auto type_ptr = ast::Type::createPointer(previous().span, lexicalScope, type_id);

    args.push_back(ast::TypedIdentifier::create(
      span + current().span,
      lexicalScope,
      ast::Identifier::create(previous().span, lexicalScope, "self"),
      type_ptr
    ));

    checkAdvance(TOKEN_COMMA);
  }

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      if (checkAdvance(TOKEN_3_DOTS)) {
        is_variadic = true;
        break;
      }

      args.push_back(parseValueDecl("for function argument name"));
    } while (checkAdvance(TOKEN_COMMA));
  }

  if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
    Error(ERROR_FN_MISSING_CLOSING_PAREN, current().span).raise();
  }

  if (checkAdvance(TOKEN_RIGHT_ARROW)) {
    return_type = parseType();
  } else {
    return_type = ast::Type::create(previous().span, lexicalScope, ast::Identifier::create(previous().span, lexicalScope, "void"));;
  }

  auto fndecl = ast::FnDecl::create(span + previous().span, lexicalScope, name, return_type, args, is_extern, is_variadic);

  if (!check(TOKEN_LEFT_BRACE)) {
    if (!checkAdvance(TOKEN_SEMICOLON)) {
      Error(ERROR_FN_MISSING_SEMICOLON, previous().span)
        .note("Function signature must be followed either by a body, or semicolon - if it's a forward-declaration")
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

  return ast::FnDef::create(span + previous().span, lexicalScope, fndecl, body);
}

std::shared_ptr<ast::Block> Parser::parseBlock(bool parseTopLevel) {
  auto span = current().span;

  if (!checkAdvance(TOKEN_LEFT_BRACE)) {
    Error(ERROR_BLOCK_MISSING_OPENING_BRACE, current().span).raise();
  }

  ast::NodeList nodes;

  while (true) {
    if (isAtEnd() || check(TOKEN_RIGHT_BRACE)) {
      break;
    }
    nodes.push_back(parseStmt(parseTopLevel));
    if (!checkAdvance(TOKEN_SEMICOLON)) {
      // Allows for '{ if (1) {} x }'
      //                         ^
      // But (should) disallow any other missing semicolons
      if (!previous().is(TOKEN_RIGHT_BRACE) && !current().is(TOKEN_RIGHT_BRACE)) {
        Error(ERROR_BLOCK_MISSING_SEMICOLON, previous().span.pointPastLast()).raise();
      }
    }
  }

  if (!checkAdvance(TOKEN_RIGHT_BRACE)) {
    Error(ERROR_BLOCK_MISSING_CLOSING_BRACE, current().span).raise();
  }

  return ast::Block::create(span + previous().span, lexicalScope, nodes);
}

std::shared_ptr<ast::Decomposition> Parser::parseDecompositionList() {
  auto span = current().span;

  if (!checkAdvance(TOKEN_LEFT_SQUARE_BRACE)) {
    Error(ERROR_DECOMPOSITION_MISSING_OPENING_SQUARE_BRACE, current().span).raise();
  }

  ast::NodeList pieces;

  do {
    pieces.push_back(check(TOKEN_LEFT_SQUARE_BRACE)
      ? ast::Node::cast(parseDecompositionList())
      : ast::Node::cast(parseIdentifier("for decomposition"))
    );
  } while (checkAdvance(TOKEN_COMMA));

  if (!checkAdvance(TOKEN_RIGHT_SQUARE_BRACE)) {
    Error(ERROR_DECOMPOSITION_MISSING_CLOSING_SQUARE_BRACE, current().span).raise();
  }

  return ast::Decomposition::create(span + previous().span, lexicalScope, pieces);
}

std::shared_ptr<ast::Node> Parser::parseVar(bool global) {
  auto span = current().span;

  if (!checkAdvance(TOKEN_VAR)) {
    Error(ERROR_VAR_MISSING_KEYWORD, current().span).raise();
  }

  if (check(TOKEN_LEFT_SQUARE_BRACE)) {
    auto decomposition = parseDecompositionList();

    if (!checkAdvance(TOKEN_EQUALS)) {
      Error(ERROR_DECOMPOSITION_MISSING_EQUALS, previous().span.pointPastLast()).raise();
    }

    decomposition->value = parseExpr();

    return decomposition;
  }

  auto valdecl = parseValueDecl("for variable name");

  return ast::VarDecl::create(span + previous().span, lexicalScope, valdecl->name, valdecl->value_type, valdecl->value, global);
}

std::shared_ptr<ast::Node> Parser::parseConst() {
  auto span = current().span;

  if (!checkAdvance(TOKEN_CONST)) {
    Error(ERROR_CONST_MISSING_KEYWORD, current().span).raise();
  }

  auto valdecl = parseValueDecl("for constant name");

  return ast::ConstDecl::create(span + previous().span, lexicalScope, valdecl->name, valdecl->value_type, valdecl->value);
}

std::shared_ptr<ast::Node> Parser::parseStruct(const ast::Node::AttributeList& attrs) {
  auto span = current().span;

  if (!checkAdvance(TOKEN_STRUCT)) {
    Error(ERROR_STRUCT_MISSING_KEYWORD, current().span).raise();
  }

  auto name = parseIdentifier("for struct name");

  std::vector<ast::Struct::GenericParam> genericTypes;

  if (checkAdvance(TOKEN_LESS)) {
    do {
      ast::Struct::GenericParam param;

      param.name = parseIdentifier("for generic type name");

      if (checkAdvance(TOKEN_EQUALS)) {
        param.default_value = parseType();
      }

      genericTypes.push_back(param);
    } while (checkAdvance(TOKEN_COMMA));

    if (!checkAdvance(TOKEN_GREATER)) {
      Error(ERROR_GENERIC_TYPE_DECL_MISSING_CLOSING_GT, previous().span.pointPastLast()).raise();
    }
  }

  if (!checkAdvance(TOKEN_LEFT_BRACE)) {
    Error(ERROR_STRUCT_MISSING_OPENING_BRACE, current().span).raise();
  }

  std::vector<std::shared_ptr<ast::TypedIdentifier>> fields;
  ast::NodeList methods;

  lexicalScope.push_back(name->value);

  bool shouldContinue = true;

  do {
    if (isAtEnd() || check(TOKEN_RIGHT_BRACE)) {
      break;
    }

    ast::Node::AttributeList attributes;

    if (check(TOKEN_LEFT_SQUARE_BRACE)) {
      attributes = parseAttributeList();
    }

    if (check(TOKEN_FN)) {
      auto fn = parseFunction(true);
      fn->attributes = attributes;
      methods.push_back(fn);
    } else {
      auto val = parseValueDecl("for struct field name");
      val->attributes = attributes;
      fields.push_back(val);
    }

    shouldContinue = previous().is(TOKEN_RIGHT_BRACE) || checkAdvance(TOKEN_SEMICOLON);
  } while (shouldContinue);

  lexicalScope.pop_back();

  if (!checkAdvance(TOKEN_RIGHT_BRACE)) {
    Error(ERROR_STRUCT_MISSING_CLOSING_BRACE, current().span).raise();
  }

  auto node = ast::Struct::create(span + previous().span, lexicalScope, name, genericTypes, fields, methods);

  if (genericTypes.empty()) {
    return node;
  }

  std::string id;
  // FIXME: For some fucking reason name->scope is empty here
  for (auto & s : lexicalScope) {
    id += s + "_";
  }

  codegen::GenericsCache::add(id + name->value, node);

  return ast::Empty::create();
}

std::shared_ptr<ast::Node> Parser::parseEnum(const ast::Node::AttributeList& attrs) {
  auto                       span = current().span;
  std::shared_ptr<ast::Node> type;
  ast::Enum::FieldList       fields;
  ast::NodeList              methods;

  if (!checkAdvance(TOKEN_ENUM)) {
    Error(ERROR_ENUM_MISSING_KEYWORD, current().span).raise();
  }

  auto name = parseIdentifier("for enum name");

  if (checkAdvance(TOKEN_COLON)) {
    type = parseType();
  } else {
    // Default enum type is i32. It's easier to create it here,
    // than to add nullptr checks all over ast::Enum methods
    type = ast::Type::create(name->span, lexicalScope, ast::Identifier::create(name->span, lexicalScope, "i32"));
  }

  if (!checkAdvance(TOKEN_LEFT_BRACE)) {
    Error(ERROR_ENUM_MISSING_OPENING_BRACE, current().span).raise();
  }

  lexicalScope.push_back(name->value);

  bool shouldContinue = true;

  do {
    if (isAtEnd() || check(TOKEN_RIGHT_BRACE)) {
      break;
    }

    if (check(TOKEN_FN)) {
      methods.push_back(parseFunction(true));
    } else {
      std::shared_ptr<ast::Identifier> field_name = parseIdentifier("for enum field name");
      std::shared_ptr<ast::Node>       field_value = nullptr;

      if (checkAdvance(TOKEN_EQUALS)) {
        field_value = parseExpr();
      }

      fields.emplace_back(field_name, field_value);
    }

    shouldContinue = previous().is(TOKEN_RIGHT_BRACE) || checkAdvance(TOKEN_COMMA);
  } while (shouldContinue);

  lexicalScope.pop_back();

  if (!checkAdvance(TOKEN_RIGHT_BRACE)) {
    Error(ERROR_ENUM_MISSING_CLOSING_BRACE, current().span).raise();
  }

  return ast::Enum::create(span + previous().span, lexicalScope, name, type, fields, methods);
}

std::shared_ptr<ast::Node> Parser::parseIf() {
  auto span = current().span;

  if (!checkAdvance(TOKEN_IF)) {
    Error(ERROR_IF_MISSING_KEYWORD, current().span).raise();
  }

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    Error(ERROR_IF_MISSING_OPENING_PAREN, current().span).raise();
  }

  std::shared_ptr<ast::Node> cond = parseExpr();

  if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
    Error(ERROR_IF_MISSING_CLOSING_PAREN, current().span).raise();
  }

  std::shared_ptr<ast::Node> then_body = parseStmt();
  std::shared_ptr<ast::Node> else_body = nullptr;

  if (checkAdvance(TOKEN_ELSE)) {
    else_body = parseStmt();
  }

  return ast::If::create(span + previous().span, lexicalScope, cond, then_body, else_body);
}

std::shared_ptr<ast::Node> Parser::parseFor() {
  auto span = current().span;

  if (!checkAdvance(TOKEN_FOR)) {
    Error(ERROR_FOR_MISSING_KEYWORD, current().span).raise();
  }

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    Error(ERROR_FOR_MISSING_OPENING_PAREN, current().span).raise();
  }

  std::shared_ptr<ast::Node> init = parseVar(false);

  if (!checkAdvance(TOKEN_SEMICOLON)) {
    Error(ERROR_FOR_MISSING_INIT_SEMICOLON, current().span).raise();
  }

  std::shared_ptr<ast::Node> cond = parseExpr();

  if (!checkAdvance(TOKEN_SEMICOLON)) {
    Error(ERROR_FOR_MISSING_COND_SEMICOLON, current().span).raise();
  }

  std::shared_ptr<ast::Node> step = parseExpr();

  if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
    Error(ERROR_FOR_MISSING_CLOSING_PAREN, current().span).raise();
  }

  std::shared_ptr<ast::Node> body = parseStmt();

  return ast::For::create(span + previous().span, lexicalScope, ast::Node::cast<ast::VarDecl>(init), cond, step, body);
}

std::shared_ptr<ast::Node> Parser::parseWhile() {
  auto span = current().span;

  if (!checkAdvance(TOKEN_WHILE)) {
    Error(ERROR_WHILE_MISSING_KEYWORD, current().span).raise();
  }

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    Error(ERROR_WHILE_MISSING_OPENING_PAREN, current().span).raise();
  }

  std::shared_ptr<ast::Node> cond = parseExpr();

  if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
    Error(ERROR_WHILE_MISSING_CLOSING_PAREN, current().span).raise();
  }

  std::shared_ptr<ast::Node> body = parseStmt();

  return ast::While::create(span + previous().span, lexicalScope, cond, body);
}

std::shared_ptr<ast::Node> Parser::parseReturn() {
  auto span = current().span;

  if (!checkAdvance(TOKEN_RETURN)) {
    Error(ERROR_RETURN_MISSING_KEYWORD, current().span).raise();
  }

  std::shared_ptr<ast::Node> expr;

  if (!check(TOKEN_SEMICOLON)) {
    expr = parseExpr();
  }

  return ast::Return::create(span + previous().span, lexicalScope, expr);
}

std::shared_ptr<ast::Node> Parser::parseUse(const ast::Node::AttributeList& attrs) {
  std::vector<std::shared_ptr<ast::Identifier>> path_nodes;
  std::vector<std::shared_ptr<ast::Identifier>> symbols;
  bool scoped = false;
  bool all    = false;

  auto span = current().span;

  if (!checkAdvance(TOKEN_USE)) {
    Error(ERROR_USE_MISSING_KEYWORD, current().span).raise();
  }

  if (checkAdvance(TOKEN_MOD)) {
    scoped = true;
  }

  path_nodes.push_back(parseIdentifier("for module name or path"));

  while (checkAdvance(TOKEN_SCOPE)) {
    if (checkAdvance(TOKEN_LEFT_BRACE)) {
      do {
        if (isAtEnd() || check(TOKEN_RIGHT_BRACE)) break;
        symbols.push_back(parseIdentifier("for import symbol"));
      } while (checkAdvance(TOKEN_COMMA));

      if (!checkAdvance(TOKEN_RIGHT_BRACE)) {
        Error(ERROR_USE_MISSING_CLOSING_BRACE, current().span).raise();
      }
      break;
    }

    if (checkAdvance(TOKEN_STAR)) {
      all = true;
      break;
    }

    path_nodes.push_back(parseIdentifier("for module path or import symbol"));
  }

  if (symbols.empty() && !all) {
    if (path_nodes.size() > 1) {
      symbols.push_back(path_nodes.back());
      path_nodes.pop_back();
    }
  }

  std::string target_prefix = path_nodes[0]->value;
  for (size_t i = 1; i < path_nodes.size(); ++i) {
    target_prefix += "_" + path_nodes[i]->value;
  }

  if (!checkAdvance(TOKEN_SEMICOLON)) {
    Error(ERROR_USE_MISSING_SEMICOLON, current().span).raise();
  }

  if (all && !symbols.empty()) {
    Error(ERROR_USE_WILDCARD_WITH_SYMBOLS, span + previous().span).raise();
  }

  auto name = path_nodes.front();

  auto path_attr = std::find_if(attrs.begin(), attrs.end(), [](auto& a) { return a.name == "path"; });
  std::string path;

  if (path_attr != attrs.end()) {
    assertRaise(path_attr->args.size() == 1, Error(ERROR_PATH_ATTR_ARG_MISMATCH, current().span));
    assertRaise(path_attr->args[0]->is(ast::AST_EXPR_STRING), Error(ERROR_PATH_ATTR_ARG_BAD_TYPE, current().span));

    path = path_attr->args[0]->as<ast::String>()->value;
  }

  auto res = path.empty() ? includeModule(name->name(), name->span, scoped) : includeModuleFromPath(name->name(), path, name->span, scoped);

  // Happens, if module was already included
  if (!res.body) {
    auto proxy_body = ast::Block::create(span, lexicalScope, {});
    auto proxy_mod = ast::Module::create(span + previous().span, lexicalScope, name, proxy_body);

    updateModAliases(proxy_mod, all, target_prefix, symbols, span);

    return proxy_mod;
  }

  auto mod = ast::Module::create(span + previous().span, lexicalScope, name, res.body);

  mod->addAttribute({"__xcc_tag_used_from", { ast::String::create(span, lexicalScope, res.path) }, span});

  updateModAliases(mod, all, target_prefix, symbols, span);

  ModuleCache::get(res.path).references.push_back(mod);

  auto prelude_attr = std::find_if(attrs.begin(), attrs.end(), [](auto& a) { return a.name == "prelude"; });

  if (prelude_attr != attrs.end()) {
    mod->addAttribute({"__xcc_tag_use_alias_prelude", {}, span});
  }

  return mod;
}

std::shared_ptr<ast::Node> Parser::parseMod(const ast::Node::AttributeList& attrs) {
  auto span = current().span;

  if (!checkAdvance(TOKEN_MOD)) {
    Error(ERROR_MOD_MISSING_KEYWORD, current().span).raise();
  }

  auto name = parseScopedIdentifier("for module name");

  if (!checkAdvance(TOKEN_LEFT_BRACE)) {
    if (!checkAdvance(TOKEN_SEMICOLON)) {
      Error(ERROR_MOD_MISSING_SEMICOLON, current().span).raise();
    }

    if (isModule) {
      // Workaround for generated `mod` from `use` having another `mod` with the same name inside
      return ast::Empty::create();
    }

    // TODO: Check if needed
    // if (!module.stack.empty()) {
    //   throw ParserException(current().line, "There can be only one file-scoped module in the file");
    // }

    // Build full lexical scope
    ast::LexicalScope declaredScope = name->scope;
    declaredScope.push_back(name->value);

    lexicalScope = declaredScope;

    auto mod_span = span + previous().span;

    // Add current module to included list, to avoid circular includes
    module.included.emplace(FileManager::get(fileId)->path);

    ast::NodeList mods = { ast::Module::create(mod_span, lexicalScope, name, ast::Block::create({}, lexicalScope, {})) };

    // Implicitly include top module
    auto top_name = declaredScope.front();
    auto res = includeModuleFromPath(top_name, resolveModulePath(top_name, mod_span), mod_span, false);

    if (res.body) {
      mods.push_back(ast::Module::create(mod_span, lexicalScope, ast::Identifier::create(mod_span, lexicalScope, top_name), res.body));
    }

    return ast::Block::create(mod_span, lexicalScope, mods);
  }

  auto body = ast::Block::create(span, lexicalScope, {});

  lexicalScope.push_back(name->value);

  while (!check(TOKEN_RIGHT_BRACE) && !isAtEnd()) {
    body->body.push_back(parseOneTopLevelNode(false, {}));
  }

  lexicalScope.pop_back();

  if (!checkAdvance(TOKEN_RIGHT_BRACE)) {
    Error(ERROR_MOD_MISSING_CLOSING_BRACE, current().span).raise();
  }

  body->span = span + previous().span;

  auto mod = ast::Module::create(span + previous().span, lexicalScope, name, body);

  auto prelude_attr = std::find_if(attrs.begin(), attrs.end(), [](auto& a) { return a.name == "prelude"; });

  if (prelude_attr != attrs.end()) {
    mod->addAttribute({"__xcc_tag_use_alias_prelude", {}, span});
  }

  return mod;
}

std::shared_ptr<ast::Node> Parser::parseTypeDeclaration(const ast::Node::AttributeList& attrs) {
  auto span = current().span;

  if (!checkAdvance(TOKEN_TYPE)) {
    Error(ERROR_TYPE_MISSING_KEYWORD, current().span).raise();
  }

  auto name = parseIdentifier("for type alias name");

  if (!checkAdvance(TOKEN_EQUALS)) {
    Error(ERROR_TYPE_MISSING_EQUALS, current().span).raise();
  }

  auto type = parseType();

  if (!checkAdvance(TOKEN_SEMICOLON)) {
    Error(ERROR_TYPE_MISSING_SEMICOLON, current().span).raise();
  }

  if (isModule) {
    module.typeAliases.push_back(name->value);
  }

  return ast::TypeDecl::create(span + previous().span, lexicalScope, name, type);
}

std::shared_ptr<ast::Node> Parser::parseMacro(const ast::Node::AttributeList& attrs) {
  auto span = current().span;

  if (!checkAdvance(TOKEN_MACRO)) {
    Error(ERROR_MACRO_MISSING_KEYWORD, current().span).raise();
  }

  auto id = parseIdentifier("for macro name");

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    Error(ERROR_MACRO_MISSING_OPENING_PAREN, current().span).raise();
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
    Error(ERROR_MACRO_MISSING_CLOSING_PAREN, current().span).raise();
  }

  auto body = parseBlock(true);

  return ast::Macro::create(span + previous().span, lexicalScope, id, args, body);
}

ast::Match::Arm Parser::parseMatchArm() {
  ast::NodeList nodes;

  do {
    if (current().is(TOKEN_NUMBER)) {
      nodes.push_back(parseNumber());
    } else if (current().is(TOKEN_IDENTIFIER)) {
      nodes.push_back(parseScopedIdentifier("for match arm condition"));
    } else {
      Error(ERROR_MATCH_INVALID_ARM_COND_TYPE, current().span)
        .note("Only identifiers and numbers are supported")
        .raise();
    }
  } while (checkAdvance(TOKEN_VERTICAL_LINE) && !current().is(TOKEN_RIGHT_ARROW) && !isAtEnd());

  if (!checkAdvance(TOKEN_RIGHT_ARROW)) {
    Error(ERROR_MATCH_MISSING_RIGHT_ARROW, current().span).raise();
  }

  auto expr = parseExpr();

  return {{nodes}, expr};
}

std::shared_ptr<ast::Node> Parser::parseMatch() {
  auto span = current().span;

  if (!checkAdvance(TOKEN_MATCH)) {
    Error(ERROR_MATCH_MISSING_KEYWORD, current().span).raise();
  }

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    Error(ERROR_MATCH_MISSING_OPENING_PAREN, current().span).raise();
  }

  std::shared_ptr<ast::Node> expr = parseExpr();

  if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
    Error(ERROR_MATCH_MISSING_CLOSING_PAREN, current().span).raise();
  }

  if (!checkAdvance(TOKEN_LEFT_BRACE)) {
    Error(ERROR_MATCH_MISSING_OPENING_BRACE, current().span).raise();
  }

  std::vector<ast::Match::Arm> arms;

  while (!check(TOKEN_RIGHT_BRACE) && !isAtEnd()) {
    arms.push_back(parseMatchArm());

    if (!arms.back().then->is(ast::AST_BLOCK) && !check(TOKEN_RIGHT_BRACE) && !checkAdvance(TOKEN_COMMA)) {
      Error(ERROR_MATCH_MISSING_COMMA_AFTER_ARM, previous().span.pointPastLast()).raise();
    }
  }

  if (!checkAdvance(TOKEN_RIGHT_BRACE)) {
    Error(ERROR_MATCH_MISSING_CLOSING_BRACE, current().span).raise();
  }

  return ast::Match::create(span + previous().span, lexicalScope, expr, arms);
}

std::shared_ptr<ast::Node> Parser::parseStmt(bool parseTopLevel) {
  switch (current().type) {
    case TOKEN_VAR:         return parseVar(false);
    case TOKEN_IF:          return parseIf();
    case TOKEN_FOR:         return parseFor();
    case TOKEN_WHILE:       return parseWhile();
    case TOKEN_RETURN:      return parseReturn();
    case TOKEN_MATCH:       return parseMatch();
    case TOKEN_EXTERN:
    case TOKEN_FN: {
      if (checkNext(TOKEN_LEFT_SQUARE_BRACE)) {
        return parseLambda();
      }

      assertRaise(parseTopLevel, Error(ERROR_INVALID_TOKEN_FOR_CONTEXT, current().span, "Unexpected 'fn' in current context"));
      return parseFunction(false);
    }
    case TOKEN_STRUCT: {
      assertRaise(parseTopLevel, Error(ERROR_INVALID_TOKEN_FOR_CONTEXT, current().span, "Unexpected 'struct' in current context"));
      return parseStruct({});
    }
    default:
      return parseExpr();
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
      expr = ast::Assign::create(expr->span + rhs->span, lexicalScope, op, expr, rhs);
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
    expr = ast::Binary::create(expr->span + rhs->span, lexicalScope, op, expr, rhs);
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseEquality() {
  auto expr = parseComparison();

  while (checkAdvanceAnyOf(TOKEN_EQUALS_EQUALS, TOKEN_NOT_EQUALS)) {
    Token op = previous();
    auto rhs = parseComparison();
    expr = ast::Binary::create(expr->span + rhs->span, lexicalScope, op, expr, rhs);
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseComparison() {
  auto expr = parseTerm();

  while (checkAdvanceAnyOf(TOKEN_GREATER, TOKEN_GREATER_EQUALS,
                               TOKEN_LESS, TOKEN_LESS_EQUALS)) {
    Token op = previous();
    auto rhs = parseTerm();
    expr = ast::Binary::create(expr->span + rhs->span, lexicalScope, op, expr, rhs);
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseTerm() {
  auto expr = parseFactor();

  while (checkAdvanceAnyOf(TOKEN_MINUS, TOKEN_PLUS)) {
    Token op = previous();
    auto rhs = parseFactor();
    expr = ast::Binary::create(expr->span + rhs->span, lexicalScope, op, expr, rhs);
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseFactor() {
  auto expr = parseCast();

  // TODO: Parse modulo ('%')
  while (checkAdvanceAnyOf(TOKEN_SLASH, TOKEN_STAR)) {
    Token op = previous();
    auto rhs = parseCast();
    expr = ast::Binary::create(expr->span + rhs->span, lexicalScope, op, expr, rhs);
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseCast() {
  auto expr = parseUnary();

  if (checkAdvance(TOKEN_AS)) {
    auto type = parseType();
    return ast::Cast::create(expr->span + type->span, lexicalScope, expr, type);
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseUnary() {
  if (checkAdvanceAnyOf(TOKEN_NOT, TOKEN_MINUS, TOKEN_TILDA,
                        TOKEN_AMP, TOKEN_STAR)) {
    Token op = previous();
    auto rhs = parseUnary();
    return ast::Unary::create(op.span + rhs->span, lexicalScope, op, rhs);
  }

  return parseSubscript();
}

std::shared_ptr<ast::Node> Parser::parseSubscript() {
  auto lhs = parseRvalue();

  while (checkAdvance(TOKEN_LEFT_SQUARE_BRACE)) {
    auto rhs = parseExpr();
    assertRaise(checkAdvance(TOKEN_RIGHT_SQUARE_BRACE), Error(ERROR_SUBSCRIPT_MISSING_CLOSING_BRACE, rhs->span.pointPastLast()));
    lhs = ast::Subscript::create(lhs->span + rhs->span, lexicalScope, lhs, rhs);
  }

  return lhs;
}

std::shared_ptr<ast::Node> Parser::parseNumber() {
  std::string value = previous().value;
  auto        span  = previous().span;

  if (value.find('.') != std::string::npos) {
    return ast::Number::createFloating(span, lexicalScope, std::stod(value));
  }

  auto res = util::determineBase(value);

  return ast::Number::createInteger(span, lexicalScope, std::stol(res.value, nullptr, res.base));
}

std::shared_ptr<ast::Node> Parser::parseRvalue() {
  if (checkAdvance(TOKEN_NUMBER)) {
    return parseNumber();
  }

  if (checkAdvance(TOKEN_STRING)) {
    return ast::String::create(previous().span, lexicalScope, previous().value);
  }

  if (checkAdvance(TOKEN_CHAR)) {
    return ast::Number::createInteger(previous().span, lexicalScope, previous().value[0]);
  }

  if (checkAdvance(TOKEN_DOLLAR_SIGN)) {
    auto span = previous().span;

    if (!checkAdvance(TOKEN_IDENTIFIER)) {
      Error(ERROR_DOLLAR_MISSING_IDENTIFIER, previous().span.pointPastLast()).raise();
    }

    auto id = previous();

    std::string str_val;

    if (checkAdvance(TOKEN_VERTICAL_LINE)) {
      if (!checkAdvance(TOKEN_STRING)) {
        Error(ERROR_ENV_VAR_MISSING_DEFAULT, current().span).raise();
      }

      str_val = previous().value;
    }

    char * value = getenv(id.value.c_str());

    if (value) {
      str_val = value;
    } else {
      assertRaise(!str_val.empty(), Error(ERROR_NO_ENV_VARIABLE, id.span, "'{}'", id.value));
    }

    return ast::String::create(span + id.span, lexicalScope, str_val);
  }

  if (checkAdvance(TOKEN_LEFT_PAREN)) {
    auto expr = parseExpr();

    if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
      Error(ERROR_EXPR_MISSING_CLOSING_PAREN, previous().span.pointPastLast()).raise();
    }

    // Allow for `(expr).method()` syntax
    if (checkAnyOf(TOKEN_DOT, TOKEN_RIGHT_ARROW)) {
      return parseMemberAccessOrLvalue(expr);
    }

    return expr;
  }

  if (check(TOKEN_LEFT_SQUARE_BRACE)) {
    // Array initializer
    return parseInitializer();
  }

  if (check(TOKEN_FN)) {
    return parseLambda();
  }

  return parseLvalueOrCallOrInitializer();
}

std::shared_ptr<ast::Node> Parser::parseLambda() {
  std::vector<std::shared_ptr<ast::TypedIdentifier>> args;
  ast::NodeList                                      captures;
  std::shared_ptr<ast::Node>                         return_type;
  bool                                               is_variadic = false;

  auto span = current().span;

  if (!checkAdvance(TOKEN_FN)) {
    Error(ERROR_LAMBDA_MISSING_KEYWORD, current().span).raise();
  }

  if (!checkAdvance(TOKEN_LEFT_SQUARE_BRACE)) {
    Error(ERROR_LAMBDA_MISSING_OPENING_SQUARE_BRACE, current().span).raise();
  }

  if (!check(TOKEN_RIGHT_SQUARE_BRACE)) {
    do {
      captures.push_back(parseExpr());
    } while (checkAdvance(TOKEN_COMMA));
  }

  if (!checkAdvance(TOKEN_RIGHT_SQUARE_BRACE)) {
    Error(ERROR_LAMBDA_MISSING_CLOSING_SQUARE_BRACE, current().span).raise();
  }

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    Error(ERROR_LAMBDA_MISSING_OPENING_PAREN, current().span).raise();
  }

  if (!check(TOKEN_RIGHT_PAREN) && !isAtEnd()) {
    do {
      if (checkAdvance(TOKEN_3_DOTS)) {
        is_variadic = true;
        break;
      }

      args.push_back(parseValueDecl("for lambda argument name"));
    } while (checkAdvance(TOKEN_COMMA));
  }

  if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
    Error(ERROR_LAMBDA_MISSING_CLOSING_PAREN, current().span).raise();
  }

  if (checkAdvance(TOKEN_RIGHT_ARROW)) {
    return_type = parseType();
  } else {
    return_type = ast::Type::create(previous().span, lexicalScope, ast::Identifier::create(previous().span, lexicalScope, "void"));;
  }

  if (!check(TOKEN_LEFT_BRACE)) {
    Error(ERROR_LAMBDA_MISSING_BODY, current().span).raise();
  }

  auto body = parseBlock();

  return ast::Lambda::create(span + previous().span, lexicalScope, captures, args, return_type, body, is_variadic);
}

std::shared_ptr<ast::Node> Parser::parseLvalueOrCallOrInitializer() {
  if (!check(TOKEN_IDENTIFIER) && !check(TOKEN_SELF)) {
    Error(ERROR_LVALUE_UNEXPECTED_TOKEN, current().span, "'{}' ({})", current().value, Token::typeToString(current().type)).raise();
  }

  /* Parse Member Access Or Plain Identifier */
  auto id = parseMemberAccessOrLvalue();

  if (check(TOKEN_LEFT_PAREN) || (check(TOKEN_NOT) && checkNext(TOKEN_LEFT_PAREN))) {
    /* Function or Macro Call */
    return parseCall(id);
  }

  if (check(TOKEN_LEFT_BRACE) && id->is(ast::AST_EXPR_IDENTIFIER)) {
    /* Struct initializer */
    return parseInitializer(ast::Node::cast<ast::Identifier>(id));
  }

  return ast::Node::cast<ast::Node>(id);
}

std::shared_ptr<ast::Node> Parser::parseMemberAccessOrLvalue(std::shared_ptr<ast::Node> id) {
  auto span = current().span;

  if (!id) {
    id = parseScopedIdentifier("for lvalue");
  }

  std::shared_ptr<ast::Node> expr = id;

  while (check(TOKEN_DOT) || check(TOKEN_RIGHT_ARROW)) {
    bool is_pointer = check(TOKEN_RIGHT_ARROW);

    advance();

    auto field = parseIdentifier("for member access");

    expr = is_pointer
      ? ast::MemberAccess::createByPointer(span + previous().span, lexicalScope, expr, field)
      : ast::MemberAccess::createByValue(span + previous().span, lexicalScope, expr, field);
  }

  /* Method Call */
  if (check(TOKEN_LEFT_PAREN)) {
    return parseCall(expr);
  }

  return expr;
}

std::shared_ptr<ast::Node> Parser::parseCall(std::shared_ptr<ast::Node> callee) {
  ast::NodeList args;

  bool isMacro = false;

  if (checkAdvance(TOKEN_NOT)) {
    assertRaise(callee->is(ast::AST_EXPR_IDENTIFIER),
      Error(ERROR_MACRO_CALLEE_IS_NOT_ID, callee->span));
    isMacro = true;
  }

  if (!checkAdvance(TOKEN_LEFT_PAREN)) {
    Error(isMacro ? ERROR_MACRO_CALL_MISSING_OPENING_PAREN : ERROR_FN_CALL_MISSING_OPENING_PAREN, current().span).raise();
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
    Error(isMacro ? ERROR_MACRO_CALL_MISSING_CLOSING_PAREN : ERROR_FN_CALL_MISSING_CLOSING_PAREN, previous().span.pointPastLast()).raise();
  }

  if (isMacro) {
    return ast::MacroCall::create(callee->span + previous().span, lexicalScope, ast::Node::cast<ast::Identifier>(callee), args);
  }

  return ast::Call::create(callee->span + previous().span, lexicalScope, callee, args);
}

std::shared_ptr<ast::Node> Parser::parseInitializer(std::shared_ptr<ast::Identifier> typeName) {
  std::vector<ast::Initializer::Value> values;
  std::shared_ptr<ast::Node>           type;
  auto                                 span = current().span;
  bool                                 has_square_braces = false;

  if (checkAdvance(TOKEN_LEFT_SQUARE_BRACE)) {
    has_square_braces = true;

    // Allow empty type '[] {...}' for inferance
    if (!checkAdvance(TOKEN_RIGHT_SQUARE_BRACE)) {
      type = parseType();

      // Parse tuples. Don't use parseType() for this, as it expects '[' not to be consumed
      // and '[]' for initializer type are required to be there, so we can't know if contents
      // of '[...]' are a tuple, or a normal type
      if (check(TOKEN_COMMA)) {
        // If set to true - will wrap type into an array in ast::Initializer::generateType
        has_square_braces = false;

        type = ast::Type::createTuple(span, lexicalScope, {type});

        while (checkAdvance(TOKEN_COMMA)) {
          type->as<ast::Type>()->tuple.members.push_back(parseType());
        }

        type->span += previous().span;
      }

      if (!checkAdvance(TOKEN_RIGHT_SQUARE_BRACE)) {
        Error(ERROR_INIT_MISSING_CLOSING_SQUARE_BRACE, current().span).raise();
      }
    }
  } else {
    type = ast::Type::create(typeName->span, lexicalScope, typeName);
  }

  span = previous().span;

  if (!checkAdvance(TOKEN_LEFT_BRACE)) {
    Error(ERROR_INIT_MISSING_OPENING_BRACE, current().span).raise();
  }

  if (!check(TOKEN_RIGHT_BRACE)) {
    do {
      if (isAtEnd() || check(TOKEN_RIGHT_BRACE)) {
        break;
      }

      ast::Initializer::Value value;

      if (checkNext(TOKEN_COLON)) {
        value.name = parseIdentifier("for field name");
        advance();
      }

      value.value = parseExpr();

      values.push_back(value);
    } while (checkAdvance(TOKEN_COMMA));
  }

  if (!checkAdvance(TOKEN_RIGHT_BRACE)) {
    Error(ERROR_INIT_MISSING_CLOSING_BRACE, current().span).raise();
  }

  return ast::Initializer::create(
    span + previous().span, lexicalScope, type, values, has_square_braces
  );
}

ast::Node::AttributeList Parser::parseAttributeList() {
  if (!checkAdvance(TOKEN_LEFT_SQUARE_BRACE)) {
    Error(ERROR_ATTR_MISSING_OPENING_BRACKET, current().span).raise();
  }

  ast::Node::AttributeList attrs;

  do {
    if (isAtEnd() || check(TOKEN_RIGHT_SQUARE_BRACE)) {
      break;
    }

    auto span = current().span;

    auto name = parseIdentifier("for attribute name");

    ast::NodeList args;

    if (checkAdvance(TOKEN_LEFT_PAREN)) {
      do {
        if (isAtEnd() || check(TOKEN_RIGHT_PAREN) || check(TOKEN_RIGHT_SQUARE_BRACE)) {
          break;
        }

        args.push_back(parseExpr());
      } while (checkAdvance(TOKEN_COMMA));

      if (!checkAdvance(TOKEN_RIGHT_PAREN)) {
        Error(ERROR_ATTR_MISSING_CLOSING_PAREN, previous().span.pointPastLast()).raise();
      }
    }

    attrs.push_back({name->name(), args, span + previous().span});
  } while (checkAdvance(TOKEN_COMMA));

  if (!checkAdvance(TOKEN_RIGHT_SQUARE_BRACE)) {
    Error(ERROR_ATTR_MISSING_CLOSING_BRACKET, previous().span.pointPastLast()).raise();
  }

  return attrs;
}

IncludedModule Parser::includeModule(
  const std::string& name,
  SourceSpan         span,
  bool               scoped
) {
  return includeModuleFromPath(name, resolveModulePath(name, span), span, scoped);
}

IncludedModule Parser::includeModuleFromPath(
  const std::string& name,
  const std::string& path,
  SourceSpan         span,
  bool               scoped
) {
  IncludedModule result;

  if (std::find(module.included.begin(), module.included.end(), path) != module.included.end()) {
    logger.warn("Skipping inclusion of '{}', as it was already included", name);
    return {};
  }

  if (ModuleCache::contains(path)) {
    logger.info("Using cached module '{}' ({})", name, path);
    module.included.emplace(path);
    return ModuleCache::get(path);
  }

  module.included.emplace(path);

  result.path = path;
  auto file = FileManager::load(result.path);

  logger.info("Found module '{}' at {}", name, result.path);

  try {
    auto lexer  = Lexer(file);
    auto tokens = lexer.tokenize();
    auto parser = Parser(file, tokens, true);

    if (scoped) {
      parser.lexicalScope = lexicalScope;
    }

    parser.lexicalScope.push_back(name);
    parser.module.searchPaths = module.searchPaths;
    parser.module.included    = module.included;

    auto mod = parser.parse(false);

    result.body = moduleReplaceDefinitions(mod);

    ModuleCache::set(path, result);

    parser.lexicalScope.pop_back();

    // Copy list of already included modules over to current parser
    module.included.merge(parser.module.included);
  } catch (CompilationException& ex) {
    ex.error.note(span, "During inclusion of module '{}' ({})", name, path).raise();
  }

  return result;
}

std::string Parser::resolveModulePath(const std::string& name, SourceSpan span) {
  namespace fs = std::filesystem;

  auto filename = name + ".xc";

  fs::path current_file(FileManager::get(fileId)->path);
  fs::path current_dir = current_file.parent_path();

  fs::path sub_target = current_dir / current_file.stem() / filename;

  if (fs::exists(sub_target)) {
    return sub_target.string();
  }

  fs::path sibling_target = current_dir / filename;

  if (fs::exists(sibling_target)) {
    return sibling_target.string();
  }

  if (fs::exists(filename)) {
    return filename;
  }

  for (auto& searchPath : module.searchPaths) {
    fs::path path = fs::path(searchPath) / filename;

    if (fs::exists(path)) {
      return path.string();
    }
  }

  Error(ERROR_MODULE_NOT_FOUND, span, "Could not resolve path to module '{}'", name).raise();
}

void Parser::updateModAliases(
  std::shared_ptr<ast::Module>&                        mod,
  bool                                                 all,
  std::string                                          target_prefix,
  const std::vector<std::shared_ptr<ast::Identifier>>& symbols,
  SourceSpan                                           span
) {
  if (all) {
    mod->addAttribute({"__xcc_tag_use_alias_all", { ast::String::create(span, mod->scope, "*"), ast::String::create(span, mod->scope, target_prefix) }, span});
  }

  for (auto& symbol : symbols) {
    mod->addAttribute({"__xcc_tag_use_alias", { ast::String::create(span, mod->scope, symbol->value), ast::String::create(span, mod->scope, target_prefix) }, symbol->span});
  }
}

std::shared_ptr<ast::Block> Parser::moduleReplaceDefinitions(const std::shared_ptr<ast::Block>& body) {
  auto result = ast::Block::create({}, lexicalScope, {});

  for (auto & node : body->body) {
    if (node->isAnyOf(ast::AST_FUNCTION_DECL, ast::AST_TYPE_DECL, ast::AST_MACRO, ast::AST_CONST_DECL)) {
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
    auto new_attrs = mergeVectors(parseAttributeList(), attrs);
    auto node      = parseOneTopLevelNode(isRepl, new_attrs);
    node->attributes.insert(node->attributes.end(), new_attrs.begin(), new_attrs.end());
    return node;
  }

  if (checkAnyOf(TOKEN_FN, TOKEN_EXTERN)) {
    return parseFunction(false);
  }

  if (check(TOKEN_VAR)) {
    auto var = parseVar(true);
    if (!checkAdvance(TOKEN_SEMICOLON)) {
      Error(ERROR_VARDECL_MISSING_SEMICOLON, previous().span.pointPastLast()).raise();
    }
    return var;
  }

  if (check(TOKEN_CONST)) {
    auto constant = parseConst();
    if (!checkAdvance(TOKEN_SEMICOLON)) {
      Error(ERROR_VARDECL_MISSING_SEMICOLON, previous().span.pointPastLast()).raise();
    }
    return constant;
  }

  if (check(TOKEN_STRUCT)) {
    return parseStruct(attrs);
  }

  if (check(TOKEN_ENUM)) {
    return parseEnum(attrs);
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
  auto block = ast::Block::create({}, lexicalScope, {});

  while (!isAtEnd()) {
    block->body.push_back(parseOneTopLevelNode(isRepl, {}));
  }

  return block;
}

void Parser::addModuleSearchPath(const std::string& path) {
  module.searchPaths.push_back(path);
}


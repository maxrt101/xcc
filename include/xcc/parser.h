#pragma once

#include "xcc/ast.h"

#include <set>

namespace xcc {

/** Forward declaration of GlobalContext to avoid circular includes */
namespace codegen { class GlobalContext; }

/**
 * Included (processed) module
 *
 * @note For internal use only
 */
struct IncludedModule {
  std::string                               path;
  std::shared_ptr<ast::Block>               body;
  std::vector<std::shared_ptr<ast::Module>> references;
  llvm::DIFile *                            di_file = nullptr;
};

/**
 * Module Cache
 *
 * @note For internal use only
 */
class ModuleCache {
public:
  static void set(const std::string& name, IncludedModule module);
  static IncludedModule& get(const std::string& name);
  static bool contains(const std::string& name);

  static void updateDebugInfo(codegen::GlobalContext& ctx);

private:
  static std::unordered_map<std::string, IncludedModule> modules;
};

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
  FileId                    fileId;
  const std::vector<Token>& tokens;       /** Token stream */
  size_t                    current_idx;  /** Index into `tokens` */
  std::vector<std::string>  structStack;  /** Stack of currently parsing struct definitions */
  bool                      isModule;

  struct {
    std::vector<std::string> searchPaths; /** List of module search paths */
    std::set<std::string>    included;    /** List of already included modules (avoid circular includes) */
    std::vector<std::string> stack;       /** Stack of currently parsing recursive mod definitions */
    std::vector<std::string> typeAliases; /** List of declared type aliases in current module */
  } module;

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

  /**
   * Parse a single-token identifier (or "self")
   *
   * @param ex_msg Message to append to "Expected identifier " if there is no identifier token
   */
  std::shared_ptr<ast::Identifier> parseIdentifier(const std::string& ex_msg);

  /**
   * Parse a single-token identifier, appending current module stack as it's scope
   *
   * So, if parser encounters this:
   * @code
   * mod test {
   *   fn do_stuff() {}
   * }
   * @endcode
   *
   * Name of function will be `test::do_stuff`
   *
   * @param ex_msg Message to append to "Expected identifier " if parsing fails
   */
  std::shared_ptr<ast::Identifier> parseIdentifierWithCurrentScope(const std::string& ex_msg);

  /**
   * Parse a scoped identifier, e.g. `test::do_stuff`
   *
   * @param ex_msg Message to append to "Expected identifier " if parsing fails
   */
  std::shared_ptr<ast::Identifier> parseScopedIdentifier(const std::string& ex_msg);

  /**
   * Parses type
   */
  std::shared_ptr<ast::Node> parseType(std::shared_ptr<ast::Identifier> name = nullptr);

  /**
   * Parses a value declaration, e.g. `a: b = c`
   */
  std::shared_ptr<ast::TypedIdentifier> parseValueDecl();

  // Statements
  std::shared_ptr<ast::Node> parseFunction(bool isMethod);
  std::shared_ptr<ast::Block> parseBlock(bool parseTopLevel = false);
  std::shared_ptr<ast::Node> parseVar(bool global);
  std::shared_ptr<ast::Node> parseConst();
  std::shared_ptr<ast::Node> parseStruct(const ast::Node::AttributeList& attrs);
  std::shared_ptr<ast::Node> parseIf();
  std::shared_ptr<ast::Node> parseFor();
  std::shared_ptr<ast::Node> parseWhile();
  std::shared_ptr<ast::Node> parseReturn();
  std::shared_ptr<ast::Node> parseUse(const ast::Node::AttributeList& attrs);
  std::shared_ptr<ast::Node> parseMod(const ast::Node::AttributeList& attrs);
  std::shared_ptr<ast::Node> parseTypeDeclaration(const ast::Node::AttributeList& attrs);
  std::shared_ptr<ast::Node> parseMacro(const ast::Node::AttributeList& attrs);

  // Generic
  std::shared_ptr<ast::Node> parseStmt(bool parseTopLevel = false);
  std::shared_ptr<ast::Node> parseExpr();

  // Expressions
  std::shared_ptr<ast::Node> parseAssignment();
  std::shared_ptr<ast::Node> parseLogicAndBitOps();
  std::shared_ptr<ast::Node> parseEquality();
  std::shared_ptr<ast::Node> parseComparison();
  std::shared_ptr<ast::Node> parseTerm();
  std::shared_ptr<ast::Node> parseFactor();
  std::shared_ptr<ast::Node> parseCast();
  std::shared_ptr<ast::Node> parseUnary();
  std::shared_ptr<ast::Node> parseSubscript();
  std::shared_ptr<ast::Node> parseNumber();
  std::shared_ptr<ast::Node> parseRvalue();
  std::shared_ptr<ast::Node> parseLvalueOrCallOrInitializer();
  std::shared_ptr<ast::Node> parseCall(std::shared_ptr<ast::Node> callee);
  std::shared_ptr<ast::Node> parseInitializer(std::shared_ptr<ast::Identifier> typeName = nullptr);

  /**
   * Parse an attribute list, that can precede any top-level declaration, e.g. `[a, b(c)]`
   */
  ast::Node::AttributeList parseAttributeList();

  /**
   * Resolves module path and calls includeModuleFromPath
   *
   * For more info look up @ref Parser::includeModuleFromPath
   *
   * @param name Module name
   * @param span Inclusion site
   * @param scoped Is scoped, i.e. should current module stack be passed to child (module) parser
   */
  IncludedModule includeModule(
    const std::string& name,
    SourceSpan         span,
    bool               scoped
  );

  /**
   * Include module from path. Reads file from path, tokenizes and parses it, stripping variable & function definitions
   * in the process. Adds this module to included list, to avoid double include. Caches parsed and processed AST and
   * uses it, if a module was already processed.
   *
   * So if there is a module `test`, that looks like this:
   * @code
   * mod test;
   *
   * struct Counter {
   *   ticks: u32;
   *
   *   fn tick(self) {
   *      self->ticks += 1;
   *   }
   * }
   *
   * fn pow(x: i32) -> i32 {
   *    return x * x;
   * }
   * @endcode
   *
   * And another file, that includes it:
   * @code
   * use test;
   *
   * fn main() -> i32 {
   *    var c: test::Counter;
   *    test::pow(4);
   * }
   * @endcode
   *
   * AST for file that includes the module `test`, will look roughly like this:
   * @code
   * [__xcc_tag_used_from("/resolved/path/to/test.xc")]
   * mod test {
   *   struct Counter {
   *      ticks: u32;
   *      fn tick(self);
   *   }
   *
   *   fn pow(x: i32) -> i32;
   * }
   *
   * fn main() -> i32 {
   *    var c: test::Counter;
   *    test::pow(4);
   * }
   * @endcode
   *
   * @param name Module name
   * @param path Path to module file
   * @param span Inclusion site
   * @param scoped Is scoped, i.e. should current module stack be passed to child (module) parser
   * @return
   */
  IncludedModule includeModuleFromPath(
    const std::string& name,
    const std::string& path,
    SourceSpan         span,
    bool               scoped
  );

  /**
   * Resolves a path to module file by name, using `this->module.searchPaths`
   *
   * To add search paths, use @ref Parser::addModuleSearchPath
   *
   * @param name Module name
   * @param span Inclusion site
   * @return Path to module file, i.e. `.../{name}.xc`
   */
  std::string resolveModulePath(const std::string& name, SourceSpan span);

  /**
   * Add alias attributes to module
   *
   * @param mod     Module
   * @param all     Add "use all" attribute
   * @param symbols Symbols to alias
   */
  void updateModAliases(
    std::shared_ptr<ast::Module>&                        mod,
    bool                                                 all,
    const std::vector<std::shared_ptr<ast::Identifier>>& symbols,
    SourceSpan                                           span
  );

  /**
   * Performs definition stripping
   *
   * For more info use @ref Parser::includeModuleFromPath
   *
   * @param body Module body
   */
  std::shared_ptr<ast::Block> moduleReplaceDefinitions(const std::shared_ptr<ast::Block>& body);

  /**
   * Parse single top-level statement/declaration
   *
   * @param attrs Attributes for node that is about to be parsed
   */
  std::shared_ptr<ast::Node> parseOneTopLevelNode(bool isRepl, const ast::Node::AttributeList& attrs);

public:
  explicit Parser(FileId fileId, const std::vector<Token>& tokens, bool isModule = false);

  /**
   * Performs parsing
   *
   * @param isRepl true if run in REPL mode
   */
  std::shared_ptr<ast::Block> parse(bool isRepl);

  /**
   * Used to append a path to a list of module search paths
   *
   * @param path Path to add
   */
  void addModuleSearchPath(const std::string& path);
};

} /* namespace xcc */

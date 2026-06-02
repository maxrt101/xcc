#include "xcc/xcc.h"
#include "xcc/attrs.h"
#include "xcc/util/fs.h"

#include <llvm/IR/LegacyPassManager.h>

#include "xcc/util/util.h"

using namespace xcc;

static auto& logger        = xcc::log::Logger::get("XCC");
static auto& lex_logger    = xcc::log::Logger::get("LEX");
static auto& ast_logger    = xcc::log::Logger::get("AST");
static auto& ex_ast_logger = xcc::log::Logger::get("AST_EXT");
static auto& ir_logger     = xcc::log::Logger::get("IR");
static auto& mod_ir_logger = xcc::log::Logger::get("IR_MOD");

/**
 * Process attributes for top-level block
 *
 * @param globalContext Global Context
 * @param block         AST Block
 */
static void processAttributes(codegen::GlobalContext& globalContext, const std::shared_ptr<ast::Block>& block) {
  // TODO: Use visitor
  for (auto& node : block->body) {
    if (!node->attributes.empty()) {
      for (auto& attr : node->attributes) {
        attr::callHandler(globalContext, attr, node.get());
      }
    }
  }
}

/**
 * Process aliases for specific modules, specifically - symbols that have to be brought into current namespace
 *
 * @param globalContext Global Context
 * @param mod           Module that has to be processed
 */
static void processModAliases(
  codegen::GlobalContext&      globalContext,
  std::shared_ptr<ast::Module> mod
) {
  using namespace xcc::ast;

  auto getExportableId = [](const std::shared_ptr<Node>& node) -> std::shared_ptr<Identifier> {
    if (node->is(AST_FUNCTION_DECL)) return Node::cast<Identifier>(node->as<FnDecl>()->name);
    if (node->is(AST_FUNCTION_DEF))  return Node::cast<Identifier>(node->as<FnDef>()->decl->name);
    if (node->is(AST_STRUCT))        return node->as<Struct>()->name;
    if (node->is(AST_TYPE_DECL))     return Node::cast<Identifier>(node->as<TypeDecl>()->name);
    if (node->is(AST_CONST_DECL))    return node->as<ConstDecl>()->name;
    if (node->is(AST_MACRO))         return node->as<Macro>()->name;
    return nullptr;
  };

  std::string local_prefix;

  for (const auto& s : mod->scope) {
    local_prefix += s + "_";
  }

  if (mod->hasAttribute("__xcc_tag_use_alias_prelude")) {
    auto attr = mod->getAttribute("__xcc_tag_use_alias_prelude");

    std::string module_absolute_prefix = local_prefix;

    if (mod->name && mod->name->is(AST_EXPR_IDENTIFIER)) {
      module_absolute_prefix += mod->name->as<Identifier>()->value + "_";
    }

    for (auto& node : mod->body->body) {
      if (auto id = getExportableId(node)) {
        std::string local_name = id->value;
        std::string target_name = module_absolute_prefix + id->value;
        globalContext.addAlias(local_name, target_name, attr.span);
      }
    }
  }

  if (mod->hasAttribute("__xcc_tag_use_alias_all")) {
    auto attr = mod->getAttribute("__xcc_tag_use_alias_all");

    std::string module_absolute_prefix = local_prefix;
    if (mod->name && mod->name->is(AST_EXPR_IDENTIFIER)) {
      module_absolute_prefix += mod->name->as<Identifier>()->value + "_";
    }

    if (mod->body) {
      for (auto& node : mod->body->body) {
        if (auto id = getExportableId(node)) {
          std::string local_name = local_prefix + id->value;
          std::string target_name = module_absolute_prefix + id->value;

          if (local_name != target_name) {
            globalContext.addAlias(local_name, target_name, attr.span);
          }
        }
      }
    }
  }

  auto symbols = mod->getAttributes("__xcc_tag_use_alias");

  for (auto& sym : symbols) {
    auto span = sym.get().span;

    if (sym.get().args.size() != 2) continue;

    std::string alias_symbol = sym.get().args[0]->as<String>()->value;
    std::string target_prefix = sym.get().args[1]->as<String>()->value;

    if (alias_symbol == "*") continue;

    std::string local_name = local_prefix + alias_symbol;
    std::string target_name = target_prefix + "_" + alias_symbol;

    if (local_name != target_name) {
      globalContext.addAlias(local_name, target_name, span);
    }
  }
}

/**
 * Process included modules, specifically - symbols that have to be brought into current namespace
 *
 * @param globalContext Global Context
 * @param block         Block to process
 */
static void processAliases(
  codegen::GlobalContext&            globalContext,
  const std::shared_ptr<ast::Block>& block
) {
  for (auto& node : block->body) {
    if (node->is(ast::AST_MOD)) {
      auto mod = ast::Node::cast<ast::Module>(node);

      globalContext.pushModule(mod->getName(), mod->getPath());
      processModAliases(globalContext, mod);
      processAliases(globalContext, mod->body);
      globalContext.popModule();
    } else if (node->is(ast::AST_BLOCK)) {
      processAliases(globalContext, ast::Node::cast<ast::Block>(node));
    }
  }
}

/**
 * Find and process all `type` and `struct` declarations, for types to be available
 * during macro expansion phase
 *
 * FIXME: What if a constant is of a custom type? (`struct A { x: i32; }; const a: A;`)
 *        Or custom type is dependant on a macro? (`struct A { x: cat!(int, _t); }`)
 *
 * @param globalContext Global Context
 * @param root          Root AST Node
 */
static void registerCustomTypes(
  codegen::GlobalContext&           globalContext,
  const std::shared_ptr<ast::Node>& root
) {
  root->visit(globalContext, [&globalContext](auto node) {
    if (node->isAnyOf(ast::AST_STRUCT, ast::AST_TYPE_DECL, ast::AST_ENUM)) {
      node->generateType(*globalContext.globalModule, {});
    }

    return nullptr;
  }, {ast::AST_MACRO, ast::AST_EXPR_MACRO_CALL});
}

/**
 * Process all constants, for them to be available during macro expansion phase
 *
 * FIXME: What if a structure's field is dependant on a constant? (`const N = 4; struct A { x: i32[N]; }`)
 *
 * @param globalContext Global Context
 * @param root          Root AST node
 */
static void registerConstants(
  codegen::GlobalContext&            globalContext,
  const std::shared_ptr<ast::Block>& root
) {
  root->visit(globalContext, [&globalContext](auto node) -> std::shared_ptr<ast::Node> {
    if (node->is(ast::AST_CONST_DECL)) {
      auto constant = ast::Node::cast<ast::ConstDecl>(node);
      globalContext.addConst(constant->name->getMangledName(constant->name->value), constant);
    }

    return nullptr;
  }, {ast::AST_MACRO});
}

/**
 * Register all function declarations/definitions
 *
 * @param globalContext Global Context
 * @param root          Root AST Node
 */
static void registerFunctions(
  codegen::GlobalContext&            globalContext,
  const std::shared_ptr<ast::Block>& root
) {
  root->visit(globalContext, [&globalContext](auto node) -> std::shared_ptr<ast::Node> {
    ast::FnDecl * decl = nullptr;

    if (node->is(ast::AST_FUNCTION_DECL)) {
      decl = node->template as<ast::FnDecl>();
    } else if (node->is(ast::AST_FUNCTION_DEF)) {
      decl = node->template as<ast::FnDef>()->decl.get();
    }

    if (decl) {
      decl->generateFunction(*globalContext.globalModule, {});
    }

    return nullptr;
  }, {ast::AST_MACRO});
}

/**
 * Register all available macros
 *
 * @param globalContext Global Context
 * @param root          Root AST Node
 */
static void registerMacros(
  codegen::GlobalContext&            globalContext,
  const std::shared_ptr<ast::Block>& root
) {
  root->visit(globalContext, [&globalContext](auto node) {
    if (node->is(ast::AST_MACRO)) {
      auto macro = ast::Node::cast<ast::Macro>(node);

      globalContext.registerMacro(macro->name->getMangledName(macro->name->value), macro);
    }

    return nullptr;
  }, {});
}

std::shared_ptr<ast::Block> xcc::moduleReplaceDefinitions(ast::LexicalScope lexicalScope, const std::shared_ptr<ast::Block>& body) {
  auto result = ast::Block::create({}, lexicalScope, {});

  for (auto & node : body->body) {
    if (node->isAnyOf(ast::AST_FUNCTION_DECL, ast::AST_TYPE_DECL, ast::AST_MACRO, ast::AST_CONST_DECL, ast::AST_EXPR_MACRO_CALL)) {
      result->body.push_back(node);
    } else if (node->is(ast::AST_MOD)) {
      auto mod = ast::Node::cast<ast::Module>(node->clone());
      mod->body = moduleReplaceDefinitions(lexicalScope, mod->body);
      result->body.push_back(mod);
    } else if (node->is(ast::AST_FUNCTION_DEF)) {
      result->body.push_back(node->as<ast::FnDef>()->decl);
    } else if (node->is(ast::AST_STRUCT)) {
      auto _struct = ast::Node::cast<ast::Struct>(node->clone());

      for (size_t j = 0; j < _struct->methods.size(); ++j) {
        _struct->methods[j] = _struct->methods[j]->as<ast::FnDef>()->decl;
      }

      result->body.push_back(_struct);
    }
  }

  return result;
}

static void compileBlock(
  codegen::GlobalContext&            globalContext,
  CompilationResult&                 result,
  const std::shared_ptr<ast::Block>& block,
  bool                               isRepl
);

/**
 * Compile function (both declarations & definitions)
 *
 * @param globalContext Global Context
 * @param node          FnDecl/FnDef node
 */
static llvm::Function * compileFunction(codegen::GlobalContext& globalContext, std::shared_ptr<ast::Node> node) {
  if (node->isAnyOf(ast::AST_FUNCTION_DEF, ast::AST_FUNCTION_DECL)) {
    auto decl = node->is(ast::AST_FUNCTION_DEF) ? node->as<ast::FnDef>()->decl.get() : node->as<ast::FnDecl>();
    auto name = decl->name->as<ast::Identifier>()->name();

    auto ctx = globalContext.createModule(name, node->span.fileId);

    auto fn = node->generateFunction(*ctx, {});

    if (ir_logger.isEnabled()) {
      ir_logger.info("LLVM IR for function {}:", fn->getName().str());
      util::RawStreamCollector fn_ir_collector;
      fn->print(*fn_ir_collector.stream());
      ir_logger.print("{}", fn_ir_collector.string());
    }

    if (mod_ir_logger.isEnabled()) {
      mod_ir_logger.info("Compiled LLVM Module for {}:", fn->getName().str());
      util::RawStreamCollector mod_ir_collector;
      ctx->llvm.module->print(*mod_ir_collector.stream(), nullptr);
      mod_ir_logger.print("{}\n", mod_ir_collector.string());
    }

    globalContext.addModule(ctx);
    return fn;
  }

  throw std::runtime_error(std::format("compileFunction expects AST_FUNCTION_DECL or AST_FUNCTION_DEF, got {}", ast::Node::typeToString(node->type)));
}

/**
 * Lower a single AST node
 *
 * @param globalContext Global Context
 * @param result        Generates functions and/or top-level expressions will be put here
 * @param node          Node to lower
 * @param isRepl        In in REPL mode
 */
static void compileNode(
  codegen::GlobalContext&           globalContext,
  CompilationResult&                result,
  const std::shared_ptr<ast::Node>& node,
  bool                              isRepl
) {
  if (node->isAnyOf(ast::AST_FUNCTION_DEF, ast::AST_FUNCTION_DECL)) {
    compileFunction(globalContext, node);
  } else if (node->is(ast::AST_VAR_DECL)) {
    node->generateValue(*globalContext.globalModule, {});
  } else if (node->is(ast::AST_CONST_DECL)) {
    auto constant = ast::Node::cast<ast::ConstDecl>(node);
    globalContext.addConst(constant->name->getMangledName(constant->name->value), constant);
  } else if (node->is(ast::AST_STRUCT)) {
    auto _struct = node->as<ast::Struct>();

    _struct->generateType(*globalContext.globalModule, {});

    _struct->generateForwardDeclarations(*globalContext.globalModule, {});

    for (auto& method : _struct->methods) {
      compileFunction(globalContext, method);
    }
  } else if (node->is(ast::AST_ENUM)) {
    node->generateType(*globalContext.globalModule, {});
    for (auto& method : node->as<ast::Enum>()->methods) {
      compileFunction(globalContext, method);
    }
  } else if (node->is(ast::AST_MOD)) {
    auto mod = ast::Node::cast<ast::Module>(node);

    globalContext.pushModule(mod->getName(), mod->getPath());
    compileBlock(globalContext, result, mod->body, isRepl);
    globalContext.popModule();
  } else if (node->is(ast::AST_TYPE_DECL)) {
    node->generateType(*globalContext.globalModule, {});
  } else if (node->is(ast::AST_BLOCK)) {
    compileBlock(globalContext, result, ast::Node::cast<ast::Block>(node), isRepl);
  } else if (node->isAnyOf(ast::AST_EMPTY, ast::AST_MACRO)) {
    // Ignore
  } else if (node->is(ast::AST_EXPR_MACRO_CALL)) {
    auto expanded = expand(node, *globalContext.globalModule);
    compileNode(globalContext, result, expanded, isRepl);
  } else {
    if (isRepl) {
      result.nodes.expr.push_back(node);
    } else {
      throw std::runtime_error("Unexpected node at top-level scope: " + ast::Node::typeToString(node->type));
    }
  }
}

/**
 * Recursively lowers AST
 *
 * @param globalContext Global Context
 * @param result        Generates functions and/or top-level expressions will be put here
 * @param block         Block to lower
 * @param isRepl        In in REPL mode
 */
static void compileBlock(
  codegen::GlobalContext&            globalContext,
  CompilationResult&                 result,
  const std::shared_ptr<ast::Block>& block,
  bool                               isRepl
) {
  for (auto& node : block->body) {
    compileNode(globalContext, result, node, isRepl);
  }
}

util::Target xcc::init(const std::string& target, const std::string& machine, bool autoCleanup) {
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  if (autoCleanup) {
    std::atexit(xcc::cleanup);
  }

  auto target_info = util::Target(target, machine);
  auto triple = llvm::Triple(target_info.target_triple);

  logger.info("XCC v{} initialized for {}", getVersion().c_str(), target_info.target_triple);

  setenv("XCC_VERSION", getVersion().c_str(), 1);
  setenv("XCC_TARGET",  target_info.target_triple.c_str(), 1);
  setenv("XCC_ARCH",    triple.getArchName().str().c_str(), 1);
  setenv("XCC_OS",      triple.getOSName().str().c_str(), 1);
  setenv("XCC_ENV",     triple.getEnvironmentName().str().c_str(), 1);
  setenv("XCC_CPU",     target_info.machine->getTargetCPU().str().c_str(), 1);

  // XCC_BUILT_ON ?
  // XCC_BUILT_BY ?

  return target_info;
}

void xcc::cleanup() {
  logger.debug("Cleaning up");
  log::cleanup();
}

CompilationResult xcc::compile(
  codegen::GlobalContext&         globalContext,
  FileId                          file,
  bool                            isRepl,
  const std::vector<std::string>& includePaths
) {
  auto lexer  = Lexer(file);
  auto tokens = lexer.tokenize();

  if (lex_logger.isEnabled()) {
    auto f = FileManager::get(file);
    lex_logger.info("TOKENS:");
    for (auto& token : tokens) {
      std::string value = token.value;
      if (token.is(TokenType::TOKEN_STRING)) {
        value = str::escseq(value, false);
      }
      lex_logger.print("{:<20} '{}' {}:{}:{} '{}'\n",
        Token::typeToString(token.type), value,
        token.span.fileId, token.span.offset, token.span.length,
        f->contents.substr(token.span.offset, token.span.length));
    }
  }

  auto parser = Parser(file, tokens);

  for (auto& path : includePaths) {
    parser.addModuleSearchPath(path);
  }

  auto ast = parser.parse(isRepl);

  if (ast_logger.isEnabled()) {
    ast_logger.info("AST (After Parsing):");
    ast_logger.print("{}\n", ast->toString(nullptr, nullptr, 0, true));
  }

  processAttributes(globalContext, ast);
  processAliases(globalContext, ast);
  registerCustomTypes(globalContext, ast);
  registerConstants(globalContext, ast);
  registerFunctions(globalContext, ast);
  registerMacros(globalContext, ast);

  if (ex_ast_logger.isEnabled()) {
    ex_ast_logger.info("AST (After Attribute & Macro processing):");
    ex_ast_logger.print("{}\n", ast->toString(nullptr, nullptr, 0, true));
  }

  CompilationResult result;

  compileBlock(globalContext, result, ast, isRepl);

  return result;
}

void xcc::compile_to_object(
  codegen::GlobalContext&         globalContext,
  FileId                          file,
  const std::string&              filename,
  const std::vector<std::string>& includePaths
) {
  std::error_code error;
  llvm::raw_fd_ostream dest(filename, error, llvm::sys::fs::OF_None);

  if (error) {
    logger.error("Could not open file {}: {}", filename, error.message());
    throw std::runtime_error("Failed to open output file");
  }

  llvm::legacy::PassManager pass;
  auto file_type = llvm::CodeGenFileType::ObjectFile;

  if (globalContext.target.machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
    logger.error("TargetMachine can't emit an object file");
    throw std::runtime_error("TargetMachine can't emit an object file");
  }

  compile(globalContext, file, false, includePaths);

  globalContext.mergeModules();

  pass.run(*globalContext.globalModule->llvm.module);
  dest.flush();
}

void xcc::run(
  codegen::GlobalContext&         globalContext,
  FileId                          file,
  bool                            isRepl,
  const std::vector<std::string>& includePaths
) {
  auto result = compile(globalContext, file, isRepl, includePaths);

  globalContext.flushModulesToJIT();

  if (isRepl) {
    if (!result.nodes.expr.empty()) {
      globalContext.runExpr(ast::Block::create(SourceSpan::builtin(), {}, result.nodes.expr));
    }
  } else {
    globalContext.runFunction("main");
  }
}

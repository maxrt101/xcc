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
 * @param block AST Block
 */
static void processAttributes(const std::shared_ptr<ast::Block>& block) {
  // TODO: Use visitor
  for (auto& node : block->body) {
    if (!node->attributes.empty()) {
      for (auto& attr : node->attributes) {
        attr::callHandler(attr, node.get());
      }
    }
  }
}

/**
 * Process aliases for specific modules, specifically - symbols that have to be brought into current namespace
 *
 * @param globalContext Global Context
 * @param mod           Module that has to be processed
 * @param topLevel      Is current context a top-level
 */
static void processModAliases(
  codegen::GlobalContext&      globalContext,
  std::shared_ptr<ast::Module> mod
) {
  using namespace xcc::ast;

  auto getExportableId = [](const std::shared_ptr<Node>& node) -> std::shared_ptr<Identifier> {
    if (node->is(AST_FUNCTION_DECL)) return node->as<FnDecl>()->name;
    if (node->is(AST_FUNCTION_DEF))  return node->as<FnDef>()->decl->name;
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

/**
 * Expand macro arguments with the ones passed via MacroCall inside of macro body
 *
 * @param macro Macro Definition
 * @param call  Macro Call
 * @param body  Macro body (cloned!)
 */
static void processMacroCall(
  codegen::GlobalContext&                globalContext,
  const std::shared_ptr<ast::Macro>&     macro,
  const std::shared_ptr<ast::MacroCall>& call,
  const std::shared_ptr<ast::Node>&      body
) {
  body->visit(globalContext, [macro, call](auto node) -> std::shared_ptr<ast::Node> {
    if (node->is(ast::AST_EXPR_IDENTIFIER)) {
      auto arg = node->template as<ast::Identifier>()->name();

      int argn = -1;

      for (int i = 0; i < macro->args.size(); ++i) {
        if (macro->args[i]->value == arg) {
          argn = i;
          break;
        }
      }

      if (argn == -1) {
        return nullptr;
      }

      return call->args[argn]->clone();
    }

    return nullptr;
  }, {});
}

/**
 * Recursively mark each generated node with provided attribute and a new span
 */
static void markExpandedMacro(
  std::shared_ptr<ast::Node>               body,
  ast::Node::Attribute                     attr,
  SourceSpan                               span
) {
  body->addAttribute(attr);
  body->span = span;

  std::unique_ptr<codegen::GlobalContext> ctx = {nullptr};

  body->visit(*ctx, [&attr, &span](auto node) -> std::shared_ptr<ast::Node> {
    if (!node->hasAttribute(attr.name)) {
      node->addAttribute(attr);
      node->span = span;
    }

    return nullptr;
  }, {});
}

/**
 * Helper that recursively parses ast::Decomposition into a phantom scope
 * For more info see @ref gatherPhantomsForMacroContext
 *
 * @param globalContext Global Context
 * @param phantoms      Phantom Scope
 * @param node          ast::Decomposition node
 */
static void addDecompositionPhantoms(
  codegen::GlobalContext&               globalContext,
  codegen::ModuleContext::PhantomScope& phantoms,
  const std::shared_ptr<ast::Node>&     node,
  std::shared_ptr<meta::Type>           parentType = nullptr
) {
  auto d = node->as<ast::Decomposition>();
  auto t = d->value ? d->generateType(*globalContext.globalModule, {}) : std::move(parentType);

  if (!t) {
    // If type for ast::Decomposition can't be generated (no value) and to parentType was provided
    // Absence of d->value means that this ast::Decomposition is a sub node of a parent ast::Decomposition
    // Absence of parentType means that this is the first call to addDecompositionPhantoms
    // If this is the first call, and node is a sub node to another ast::Decomposition, it means
    // that the parent (along with current) ast::Decomposition was already processed

    // TODO: Add some kind of Node::markVisited to exclude a subtree from being visited again.
    //       This might be challenging as visit() and callVisitor will need some kind of context
    //       and this context (containing visited nodes) should be able to efficiently check
    //       if the node was processed (maybe a O(1) access map/set with pointers to nodes? or add
    //       a global node instance id)

    return;
  }

  for (size_t i = 0; i < d->pieces.size(); ++i) {
    auto& piece = d->pieces[i];

    if (piece->is(ast::AST_EXPR_IDENTIFIER)) {
      phantoms.add(
        piece->as<ast::Identifier>()->name(),
        d->generateTypeForPiece(t, i)
      );
    } else {
      addDecompositionPhantoms(globalContext, phantoms, piece, t);
    }
  }
}

/**
 * Helper for gathering any and all declared variables into phantom scope,
 * for native macros to have access to them, before actual AST lowering is done.
 * Adds found variable declarations to @ref codegen::ModuleContext::PhantomScope.
 * Old variables will get overwritten. There is a side effect - variable
 * declarations will be accessible by macros outside variable's lexical scope.
 *
 * @warning Strictly internal
 *
 * @param globalContext Global Context
 * @param phantoms      Phantom Scope
 * @param node          AST Node to check
 */
static void gatherPhantomsForMacro(
  codegen::GlobalContext&               globalContext,
  codegen::ModuleContext::PhantomScope& phantoms,
  std::shared_ptr<ast::Node>            node
) {
  ast::Node::callVisitor(globalContext, node, [&globalContext, &phantoms](auto node) -> std::shared_ptr<ast::Node> {
    if (node->is(ast::AST_STRUCT)) {
    }

    if (node->is(ast::AST_VAR_DECL)) {
      phantoms.add(node->template as<ast::VarDecl>()->name->name(), node->generateType(*globalContext.globalModule, {}));
    }

    if (node->is(ast::AST_DECOMPOSITION_DECL)) {
      addDecompositionPhantoms(globalContext, phantoms, node);
    }

    if (node->is(ast::AST_FUNCTION_DECL)) {
      auto fndecl = node->template as<ast::FnDecl>();

      phantoms.add(fndecl->name->name(), node->generateType(*globalContext.globalModule, {}));

      for (auto& arg : fndecl->args) {
        phantoms.add(arg->name->name(), arg->generateType(*globalContext.globalModule, {}));
      }
    }

    return nullptr;
  }, {ast::AST_MACRO});
}

void xcc::processMacros(
  codegen::GlobalContext&    globalContext,
  std::shared_ptr<ast::Node> root
) {
  auto phantoms = globalContext.globalModule->phantomScope({});

  root->visit(globalContext, [&globalContext, &phantoms](auto node) -> std::shared_ptr<ast::Node> {
    gatherPhantomsForMacro(globalContext, phantoms, node);

    if (!node->is(ast::AST_EXPR_MACRO_CALL)) {
      return nullptr;
    }

    auto call  = ast::Node::cast<ast::MacroCall>(node);
    auto name  = call->name->getResolvedName(*globalContext.globalModule);
    auto macro = globalContext.getMacro(name);

    assertRaise(macro != nullptr, Error(ERROR_UNKNOWN_MACRO, call->name->span, "'{}'", name));

    if (macro->variadic) {
      assertRaise(macro->args.size() <= call->args.size(), Error(ERROR_MACRO_CALL_ARG_COUNT_MISMATCH, call->span, "'{}'", name));
    } else {
      assertRaise(macro->args.size() == call->args.size(), Error(ERROR_MACRO_CALL_ARG_COUNT_MISMATCH, call->span, "'{}'", name));
    }

    if (macro->native) {
      auto res = macro->fn(globalContext, call);

      // Attach expansion markers & set span to call site
      markExpandedMacro(res, {
        "__xcc_macro_expanded_from",
        {ast::Identifier::create(macro->span, macro->scope, name)},
        call->span
      }, call->span);

      return res;
    }

    auto body = ast::Node::cast<ast::Block>(macro->body->clone());

    try {
      processMacroCall(globalContext, macro, call, body);
      registerCustomTypes(globalContext, body);

      processMacros(globalContext, body);
    } catch (CompilationException& ex) {
      ex.error.note(macro->span, "During expansion of macro {}", name).raise();
    }

    // Attach expansion markers & set span to call site
    markExpandedMacro(body, {
      "__xcc_macro_expanded_from",
      {ast::Identifier::create(macro->span, macro->scope, name)},
      call->span
    }, call->span);

    return body;
  }, {ast::AST_MACRO});
}

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

    auto ctx = globalContext.createModule(name);

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
    if (node->isAnyOf(ast::AST_FUNCTION_DEF, ast::AST_FUNCTION_DECL)) {
      compileFunction(globalContext, node);
    } else if (node->is(ast::AST_VAR_DECL)) {
      node->generateValue(*globalContext.globalModule, {});
    } else if (node->is(ast::AST_CONST_DECL)) {
      auto constant = ast::Node::cast<ast::ConstDecl>(node);
      globalContext.addConst(constant->name->getMangledName(constant->name->value), constant);
    } else if (node->is(ast::AST_STRUCT)) {
      node->generateType(*globalContext.globalModule, {});
      for (auto& method : node->as<ast::Struct>()->methods) {
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
    } else {
      if (isRepl) {
        result.nodes.expr.push_back(node);
      } else {
        throw std::runtime_error("Unexpected node at top-level scope: " + ast::Node::typeToString(node->type));
      }
    }
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

  logger.info("XCC v{} initialized for {}", getVersion().c_str(), target_info.target_triple);

  setenv("XCC_VERSION", getVersion().c_str(), 1);
  // XCC_TARGET
  // XCC_ARCH
  // XCC_MACH
  // XCC_OS

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
  globalContext.createCompileUnit(file);

  auto lexer  = Lexer(file);
  auto tokens = lexer.tokenize();

  if (lex_logger.isEnabled()) {
    auto f = FileManager::get(file);
    lex_logger.info("TOKENS:");
    for (auto& token : tokens) {
      std::string value = token.value;
      if (token.is(TokenType::TOKEN_STRING)) {
        value = util::strescseq(value, false);
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

  ModuleCache::updateDebugInfo(globalContext);

  auto ast = parser.parse(isRepl);

  if (ast_logger.isEnabled()) {
    ast_logger.info("AST (After Parsing):");
    ast_logger.print("{}\n", ast->toString(nullptr, nullptr, 0, true));
  }

  processAttributes(ast);
  processAliases(globalContext, ast);
  registerCustomTypes(globalContext, ast);
  registerConstants(globalContext, ast);
  registerFunctions(globalContext, ast);
  registerMacros(globalContext, ast);
  processMacros(globalContext, ast);

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

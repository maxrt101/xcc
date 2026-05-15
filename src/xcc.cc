#include "xcc/xcc.h"
#include "xcc/attrs.h"
#include "xcc/util/fs.h"

#include <llvm/IR/LegacyPassManager.h>

using namespace xcc;

static auto logger = xcc::util::log::Logger("XCC");

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
  std::unique_ptr<codegen::GlobalContext>& globalContext,
  std::shared_ptr<ast::Module>             mod,
  bool                                     topLevel
) {
  using namespace xcc::ast;

  auto prefix = topLevel ? globalContext->getModulePrefix() : globalContext->getParentModulePrefix();

  if (mod->hasAttribute("__xcc_tag_use_alias_all")) {
    for (auto& node : mod->body->body) {
      auto attr = mod->getAttribute("__xcc_tag_use_alias_all");
      std::shared_ptr<Identifier> id;

      if (node->is(AST_FUNCTION_DECL)) {
        id = node->as<FnDecl>()->name;
      } else if (node->is(AST_FUNCTION_DEF)) {
        id = node->as<FnDef>()->decl->name;
      } else if (node->is(AST_STRUCT)) {
        id = node->as<Struct>()->name;
      } else if (node->is(AST_TYPE_DECL)) {
        id = Node::cast<Identifier>(node->as<TypeDecl>()->name);
      } else if (node->is(AST_CONST_DECL)) {
        id = node->as<ConstDecl>()->name;
      } else if (node->is(AST_MACRO)) {
        id = node->as<Macro>()->name;
      } else {
        continue;
      }

      globalContext->addAlias(prefix + id->value, id->name(), attr.span);
    }
  }

  auto symbols = mod->getAttributes("__xcc_tag_use_alias");

  for (auto& sym : symbols) {
    auto span = sym.get().span;
    auto arg = sym.get().args[0];

    assertRaiseFromNode(
      arg->is(AST_EXPR_STRING),
      Error(ERROR_ATTR_ARG_TYPE_MISMATCH, span, "__xcc_tag_use_alias expects a string as an argument"),
      mod.get()
    );

    auto name = arg->as<String>()->value;

    globalContext->addAlias(name, prefix + name, span);
  }
}

/**
 * Process included modules, specifically - symbols that have to be brought into current namespace
 *
 * @param globalContext Global Context
 * @param block         Block to process
 * @param topLevel      Is block at the very top level
 */
static void processAliases(
  std::unique_ptr<codegen::GlobalContext>& globalContext,
  const std::shared_ptr<ast::Block>&       block,
  bool                                     topLevel
) {
  for (auto& node : block->body) {
    if (node->is(ast::AST_MOD)) {
      auto mod = ast::Node::cast<ast::Module>(node);

      globalContext->pushModule(mod->getName());
      processModAliases(globalContext, mod, topLevel);
      processAliases(globalContext, mod->body, false);
      globalContext->popModule();
    } else if (node->is(ast::AST_BLOCK)) {
      processAliases(globalContext, ast::Node::cast<ast::Block>(node), false);
    }
  }
}

/**
 * Find and process all `type` and `struct` declarations, for types to be available
 * during macro expansion phase
 *
 * FIXME: What if a constant is of a custom type? (`struct A { x: i32; }; const a: A;`)
 *
 * @param globalContext Global Context
 * @param root          Root AST Node
 */
static void registerCustomTypes(
  std::unique_ptr<codegen::GlobalContext>& globalContext,
  const std::shared_ptr<ast::Block>&       root
) {
  root->visit([&globalContext](auto node) {
    if (node->isAnyOf(ast::AST_STRUCT, ast::AST_TYPE_DECL)) {
      node->generateType(*globalContext->globalModule, {});
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
  std::unique_ptr<codegen::GlobalContext>& globalContext,
  const std::shared_ptr<ast::Block>& root
) {
  root->visit([&globalContext](auto node) -> std::shared_ptr<ast::Node> {
    if (node->is(ast::AST_CONST_DECL)) {
      auto constant = ast::Node::cast<ast::ConstDecl>(node);
      globalContext->addConst(constant->name->name(), constant);
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
  std::unique_ptr<codegen::GlobalContext>& globalContext,
  const std::shared_ptr<ast::Block>&       root
) {
  root->visit([&globalContext](auto node) {
    if (node->is(ast::AST_MACRO)) {
      auto macro = ast::Node::cast<ast::Macro>(node);

      globalContext->registerMacro(macro->name->name(), macro);
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
  std::shared_ptr<ast::Macro>     macro,
  std::shared_ptr<ast::MacroCall> call,
  std::shared_ptr<ast::Node>      body
) {
  body->visit([macro, call](auto node) -> std::shared_ptr<ast::Node> {
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
static void markExpandedMacro(std::shared_ptr<ast::Node> body, ast::Node::Attribute attr, SourceSpan span) {
  body->addAttribute(attr);
  body->span = span;

  body->visit([&attr, &span](auto node) -> std::shared_ptr<ast::Node> {
    if (!node->hasAttribute(attr.name)) {
      node->addAttribute(attr);
      node->span = span;
    }

    return nullptr;
  }, {});
}

/**
 * Find and expand all ast::MacroCall nodes
 *
 * @param ctx  NativeContext for macro
 * @param root Root AST node
 */
static void processMacros(
  ast::Macro::NativeContext&         ctx,
  const std::shared_ptr<ast::Block>& root
) {
  root->visit([&ctx](auto node) -> std::shared_ptr<ast::Node> {
    if (node->is(ast::AST_VAR_DECL)) {
      ctx.vardecls[node->template as<ast::VarDecl>()->name->name()] = node;
    }

    if (node->is(ast::AST_FUNCTION_DECL)) {
      auto fndecl = node->template as<ast::FnDecl>();

      ctx.fndecls[fndecl->name->name()] = node;

      // Save argument declarations. Old ones will get overwritten.
      // There is a side effect - arguments will be accessible outside of function definition
      // TODO: Maybe tried a combined approach - if node is fndef - run recursively, saving
      //       args into a stack, if not - process by visit()
      for (auto& arg : fndecl->args) {
        ctx.args[arg->name->name()] = arg;
      }
    }

    if (node->is(ast::AST_EXPR_MACRO_CALL)) {
      auto call  = ast::Node::cast<ast::MacroCall>(node);
      auto name  = ctx.global.aliased(call->name->name());
      auto macro = ctx.global.getMacro(name);

      assertRaise(macro != nullptr, Error(ERROR_UNKNOWN_MACRO, call->name->span, "'{}'", name));

      if (macro->variadic) {
        assertRaise(macro->args.size() <= call->args.size(), Error(ERROR_MACRO_CALL_ARG_COUNT_MISMATCH, call->span, "'{}'", name));
      } else {
        assertRaise(macro->args.size() == call->args.size(), Error(ERROR_MACRO_CALL_ARG_COUNT_MISMATCH, call->span, "'{}'", name));
      }

      if (macro->native) {
        auto res = macro->fn(ctx, call);

        // Attach expansion markers & set span to call site
        markExpandedMacro(res, {
          "__xcc_macro_expanded_from",
          {ast::Identifier::create(macro->span, name)},
          call->span
        }, call->span);

        return res;
      }

      auto body = ast::Node::cast<ast::Block>(macro->body->clone());

      try {
        processMacroCall(macro, call, body);

        // Include current macro's args into expansion of inner macros
        ast::Macro::NativeContext mctx = ctx;
        for (size_t i = 0; i < macro->args.size(); ++i) {
          mctx.args[macro->args[i]->template as<ast::Identifier>()->name()] = call->args[i];
        }

        processMacros(mctx, body);
      } catch (CompilationException& ex) {
        ex.error.note(macro->span, "During expansion of macro {}", name).raise();
      }

      // Attach expansion markers & set span to call site
      markExpandedMacro(body, {
        "__xcc_macro_expanded_from",
        {ast::Identifier::create(macro->span, name)},
        call->span
      }, call->span);

      return body;
    }

    return nullptr;
  }, {ast::AST_MACRO});
}

/**
 * Compile function (both declarations & definitions)
 *
 * @param globalContext Global Context
 * @param node          FnDecl/FnDef node
 */
static llvm::Function * compileFunction(std::unique_ptr<codegen::GlobalContext>& globalContext, std::shared_ptr<ast::Node> node) {
  if (node->isAnyOf(ast::AST_FUNCTION_DEF, ast::AST_FUNCTION_DECL)) {
    auto decl = node->is(ast::AST_FUNCTION_DEF) ? node->as<ast::FnDef>()->decl.get() : node->as<ast::FnDecl>();
    auto name = decl->name->as<ast::Identifier>()->name();

    auto ctx = globalContext->createModule(name);

    auto fn = node->generateFunction(*ctx, {});

#if USE_PRINT_FUNCTION_LLVM_IR
    logger.info("LLVM IR for function {}:", fn->getName().str());
    util::RawStreamCollector fn_ir_collector;
    fn->print(*fn_ir_collector.stream());
    logger.print("{}", fn_ir_collector.string());
#endif

#if USE_PRINT_MODULE_LLVM_IR
    util::RawStreamCollector mod_ir_collector;
    ctx->llvm.module->print(*mod_ir_collector.stream(), nullptr);
    logger.info("Compiled LLVM Module for {}:", fn->getName().str());
    logger.print("{}\n", mod_ir_collector.string());
#endif

    globalContext->addModule(ctx);
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
  std::unique_ptr<codegen::GlobalContext>& globalContext,
  CompilationResult&                       result,
  const std::shared_ptr<ast::Block>&       block,
  bool                                     isRepl
) {
  for (auto& node : block->body) {
    if (node->isAnyOf(ast::AST_FUNCTION_DEF, ast::AST_FUNCTION_DECL)) {
      compileFunction(globalContext, node);
    } else if (node->is(ast::AST_VAR_DECL)) {
      node->generateValue(*globalContext->globalModule, {});
    } else if (node->is(ast::AST_CONST_DECL)) {
      auto constant = ast::Node::cast<ast::ConstDecl>(node);
      globalContext->addConst(constant->name->name(), constant);
    } else if (node->is(ast::AST_STRUCT)) {
      node->generateType(*globalContext->globalModule, {});
      for (auto& method : node->as<ast::Struct>()->methods) {
        compileFunction(globalContext, method);
      }
    } else if (node->is(ast::AST_MOD)) {
      auto mod = ast::Node::cast<ast::Module>(node);

      globalContext->pushModule(mod->getName());
      compileBlock(globalContext, result, mod->body, isRepl);
      globalContext->popModule();
    } else if (node->is(ast::AST_TYPE_DECL)) {
      node->generateType(*globalContext->globalModule, {});
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

  return target_info;
}

void xcc::cleanup() {
  logger.debug("Cleaning up");
  util::log::cleanup();
}

CompilationResult xcc::compile(
  std::unique_ptr<codegen::GlobalContext>& globalContext,
  FileId                                   file,
  bool                                     isRepl,
  const std::vector<std::string>&          includePaths
) {
  globalContext->createCompileUnit(file);

  auto lexer  = Lexer(file);
  auto tokens = lexer.tokenize();

#if USE_PRINT_TOKENS
  auto f = FileManager::get(file);
  logger.info("TOKENS:");
  for (auto& token : tokens) {
    std::string value = token.value;
    if (token.is(TokenType::TOKEN_STRING)) {
      value = util::strescseq(value, false);
    }
    logger.print("{:<20} '{}' {}:{}:{} '{}'\n",
      Token::typeToString(token.type), value,
      token.span.fileId, token.span.offset, token.span.length,
      f->contents.substr(token.span.offset, token.span.length));
  }
#endif

  auto parser = Parser(file, tokens);

  for (auto& path : includePaths) {
    parser.addModuleSearchPath(path);
  }

  ModuleCache::updateDebugInfo(*globalContext);

  auto ast = parser.parse(isRepl);

#if USE_PRINT_AST
  logger.info("AST (After Parsing):");
  logger.print("{}\n", ast->toString(nullptr, nullptr, 0, true));
#endif

  auto mctx = ast::Macro::NativeContext {*globalContext};

  processAttributes(ast);
  processAliases(globalContext, ast, true);
  registerCustomTypes(globalContext, ast);
  registerConstants(globalContext, ast);
  registerMacros(globalContext, ast);
  processMacros(mctx, ast);

#if USE_PRINT_EXPANDED_AST
  logger.info("AST (After Attribute & Macro processing):");
  logger.print("{}\n", ast->toString(nullptr, nullptr, 0, true));
#endif

  CompilationResult result;

  compileBlock(globalContext, result, ast, isRepl);

  return result;
}

void xcc::compile_to_object(
  std::unique_ptr<codegen::GlobalContext>& globalContext,
  FileId                                   file,
  const std::string&                       filename,
  const std::vector<std::string>&          includePaths
) {
  std::error_code error;
  llvm::raw_fd_ostream dest(filename, error, llvm::sys::fs::OF_None);

  if (error) {
    logger.error("Could not open file {}: {}", filename, error.message());
    throw std::runtime_error("Failed to open output file");
  }

  llvm::legacy::PassManager pass;
  auto file_type = llvm::CodeGenFileType::ObjectFile;

  if (globalContext->target.machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
    logger.error("TargetMachine can't emit an object file");
    throw std::runtime_error("TargetMachine can't emit an object file");
  }

  compile(globalContext, file, false, includePaths);

  globalContext->mergeModules();

  pass.run(*globalContext->globalModule->llvm.module);
  dest.flush();
}

void xcc::run(
  std::unique_ptr<codegen::GlobalContext>& globalContext,
  FileId                                   file,
  bool                                     isRepl,
  const std::vector<std::string>&          includePaths
) {
  auto result = compile(globalContext, file, isRepl, includePaths);

  globalContext->flushModulesToJIT();

  if (isRepl) {
    if (!result.nodes.expr.empty()) {
      globalContext->runExpr(ast::Block::create(SourceSpan::builtin(), result.nodes.expr));
    }
  } else {
    globalContext->runFunction("main");
  }
}

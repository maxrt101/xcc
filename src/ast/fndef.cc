#include "xcc/ast/fndef.h"
#include "xcc/ast/string.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/util/log.h"

#include <llvm/Transforms/Utils/ModuleUtils.h>

using namespace xcc;
using namespace xcc::ast;

static auto& logger = xcc::log::Logger::get("FN", log::Flag::SPLIT_ON_NEWLINE);

FnDef::FnDef(SourceSpan span, LexicalScope scope, std::shared_ptr<FnDecl> decl, std::shared_ptr<Block> body)
  : Node(AST_FUNCTION_DEF, span, scope), decl(std::move(decl)), body(std::move(body)) {}

std::shared_ptr<FnDef> FnDef::create(SourceSpan span, LexicalScope scope, std::shared_ptr<FnDecl> decl, std::shared_ptr<Block> body) {
  return std::make_shared<FnDef>(span, scope, std::move(decl), std::move(body));
}

std::shared_ptr<Node> FnDef::clone() {
  return withAttrs(create(
    span, scope,
    cast<FnDecl>(decl->clone()),
    cast<Block>(body->clone())
  ));
}

void FnDef::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(globalContext, decl, visitor, ignoreSubtree);
  callVisitor(globalContext, body, visitor, ignoreSubtree);
}

std::string FnDef::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(indent, newline) + std::format("{} {}",
    decl->toString(parent, this, indent, newline),
    body->toString(parent, this, indent, newline)
  );
}

llvm::Function * FnDef::generateFunction(codegen::ModuleContext& ctx, PayloadList payload) {
  auto gen_fn = decl->generateFunction(ctx, payload);

  auto meta_fn = ctx.globalContext.getMetaFunction(gen_fn->getName().str());

  auto fn = ctx.getFunction(gen_fn->getName().str());

  if (!fn) {
    Error(ERROR_INTERNAL_FAILURE, decl->span, "Error generating Function object for '{}'", fn->getName().str()).raiseFromNode(this);
  }

  // Avoid generating the function body twice
  if (!gen_fn->isDeclaration()) {
    return gen_fn;
  }

  auto di_fn = ctx.di_builder->createFunction(
    ctx.currentDIScope(), // or ctx.currentScope().di_scope
    meta_fn->name,
    meta_fn->name,
    ctx.getCurrentDIFile(),
    span.start().line,
    generateType(ctx, payload)->getDISubroutineType(ctx),
    body->span.start().line,
    llvm::DINode::FlagPrototyped,
    llvm::DISubprogram::SPFlagDefinition
  );

  fn->setSubprogram(di_fn);

  ctx.setDebugLocation(span, di_fn);

  auto basic_block = llvm::BasicBlock::Create(*ctx.llvm.ctx, "entry", fn);

  ctx.ir_builder->SetInsertPoint(basic_block);

  if (hasAttribute("naked")) {
    generateNakedFunction(ctx, payload, meta_fn, fn, di_fn);
  } else {
    generateNormalFunction(ctx, payload, meta_fn, fn, di_fn);
  }

  processAttributes(ctx, payload, fn);

  ctx.globalContext.clearCurrentFunction();

  util::RawStreamCollector collector;
  if (llvm::verifyFunction(*fn, collector.stream())) {
#if USE_PRINT_LLVM_IR_ON_VERIFY_FAIL
    util::RawStreamCollector fn_collector;
    fn->print(*fn_collector.stream());
    logger.debug("Function '{}' IR:", meta_fn->name);
    logger.print("{}", fn_collector.string());
#endif
    Error(ERROR_LLVM_ERROR, decl->span, "Function '{}' didn't pass validation", fn->getName().str())
      .note("{}", std::string(collector.string()))
      .raiseFromNode(this);
  }

  return fn;
}

std::shared_ptr<meta::Type> FnDef::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  return decl->generateType(ctx, payload);
}

void FnDef::processAttributes(codegen::ModuleContext& ctx, PayloadList payload, llvm::Function * fn) {
  if (hasAttribute("section")) {
    auto attr = getAttribute("section");

    attr.validateArgsStrict({AST_EXPR_STRING});

    auto section_name = attr.args[0]->as<ast::String>()->value;

    logger.debug("Placing '{}' in section '{}'", decl->name->name(), section_name);

    fn->setSection(section_name);
  }

  if (hasAttribute("inline")) {
    auto attr = getAttribute("inline");

    enum {
      INLINE_DEFAULT,
      INLINE_ALLWAYS,
      INLINE_NEVER,
    } inline_type = INLINE_DEFAULT;

    if (attr.args.size()) {
      attr.validateArgsStrict({AST_EXPR_IDENTIFIER});

      auto value = attr.args[0]->as<Identifier>()->value;

      if (value == "allways") {
        inline_type = INLINE_ALLWAYS;
      } else if (value == "never") {
        inline_type = INLINE_NEVER;
      } else {
        Error(ERROR_INLINE_ATTR_INVALID_VALUE, attr.span, "'{}'", value).raiseFromNode(this);
      }
    }

    switch (inline_type) {
      case INLINE_DEFAULT:
        fn->addFnAttr(llvm::Attribute::InlineHint);
        break;
      case INLINE_ALLWAYS:
        fn->addFnAttr(llvm::Attribute::AlwaysInline);
        break;
      case INLINE_NEVER:
        fn->addFnAttr(llvm::Attribute::NoInline);
        break;
    }
  }

  if (hasAttribute("noreturn")) {
    fn->addFnAttr(llvm::Attribute::NoReturn);
  }

  if (hasAttribute("weak")) {
    fn->setLinkage(decl->isExtern ? llvm::Function::ExternalWeakLinkage : llvm::Function::WeakAnyLinkage);
  }

  if (hasAttribute("naked")) {
    fn->addFnAttr(llvm::Attribute::Naked);
  }

  if (hasAttribute("returns_twice")) {
    fn->addFnAttr(llvm::Attribute::ReturnsTwice);
  }

  if (hasAttribute("constructor")) {
    auto attr = getAttribute("constructor");

    assertRaise(attr.args.size() == 1,
      Error(ERROR_ATTR_ARG_COUNT_MISMATCH, span, "Attribute 'constructor' expected 1 arg, got {}", attr.args.size()));

    auto prio = llvm::dyn_cast<llvm::ConstantInt>(attr.args[0]->generateConstant(ctx, payload));

    assertRaise(prio, Error(ERROR_NOT_CONSTANT, attr.args[0]->span,
      "Attribute 'constructor' must receive a constant expression as an argument"));

    llvm::appendToGlobalCtors(*ctx.llvm.module, fn, prio->getValue().getLimitedValue());
  }
}

void FnDef::generateNakedFunction(
    codegen::ModuleContext&                ctx,
    PayloadList                            payload,
    const std::shared_ptr<meta::Function>& meta_fn,
    llvm::Function *                       fn,
    llvm::DISubprogram *                   di_fn
) {
  ctx.pushScope(span, di_fn);

  ctx.globalContext.setCurrentFunction(decl->name->name());

  for (auto& node : body->body) {
    assertRaiseFromNode(node->is(AST_ASM), Error(ERROR_NOT_ASM_IN_NAKED_FN, node->span), this);

    node->generateValue(ctx, payload);
  }

  ctx.ir_builder->CreateUnreachable();

  ctx.popScope();
}

void FnDef::generateNormalFunction(
    codegen::ModuleContext&                ctx,
    PayloadList                            payload,
    const std::shared_ptr<meta::Function>& meta_fn,
    llvm::Function *                       fn,
    llvm::DISubprogram *                   di_fn
) {
  // Create a separate scope for function arguments
  ctx.pushScope(span, di_fn);

  for (auto& arg : fn->args()) {
    auto arg_name = std::string(arg.getName());
    auto span = decl->getArgument(arg_name)->span;
    ctx.addLocal(arg_name, meta::TypedValue::create(ctx, fn, span, meta_fn->args[arg_name], arg_name));

    ctx.addDIParameter(di_fn, arg_name, meta_fn->args[arg_name], span, arg.getArgNo() + 1);

    ctx.ir_builder->CreateStore(&arg, ctx.getLocalValue(arg_name));
  }

  ctx.globalContext.setCurrentFunction(decl->name->name());

  auto last_val = body->generateValue(ctx, extendPayload(payload, Block::Payload::create(meta_fn->returnType, true)));

  // Check if last node of function is `return`
  bool hadReturnAsLastNode = body->body.empty() ? false : isOrIsLastInBlock(body->body.back(), AST_RETURN);

  // Pop function scope
  // If last stmt is `return` - scope is already cleared, so pass this value as `no_clear` arg
  ctx.popScope(hadReturnAsLastNode);

  // If there was no explicit return - create one with the last value of function block as result
  if (!hadReturnAsLastNode) {
    if (meta_fn->returnType->isVoid()) {
      ctx.ir_builder->CreateRetVoid();
    } else {
      try {
        last_val = castIfNotSame(ctx, last_val, meta_fn->getLLVMReturnType(ctx), body->body.back()->span);
      } catch (CompilationException& ex) {
        ex.error
          .note(decl->span, "Function expected a '{}'; got here by treating the last value of block as return value", meta_fn->returnType->toString())
          .raise();
      }

      ctx.ir_builder->CreateRet(last_val);
    }
  }
}

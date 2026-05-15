#include "xcc/ast/fndef.h"
#include "xcc/ast/string.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/util/log.h"

#include <llvm/Transforms/Utils/ModuleUtils.h>

using namespace xcc;
using namespace xcc::ast;

static auto logger = xcc::util::log::Logger("FNDEF", util::log::Flag::SPLIT_ON_NEWLINE);

FnDef::FnDef(SourceSpan span, std::shared_ptr<FnDecl> decl, std::shared_ptr<Block> body)
  : Node(AST_FUNCTION_DEF, span), decl(std::move(decl)), body(std::move(body)) {}

std::shared_ptr<FnDef> FnDef::create(SourceSpan span, std::shared_ptr<FnDecl> decl, std::shared_ptr<Block> body) {
  return std::make_shared<FnDef>(span, std::move(decl), std::move(body));
}

std::shared_ptr<Node> FnDef::clone() {
  return withAttrs(create(
    span,
    cast<FnDecl>(decl->clone()),
    cast<Block>(body->clone())
  ));
}

void FnDef::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(decl, visitor, ignoreSubtree);
  callVisitor(body, visitor, ignoreSubtree);
}

std::string FnDef::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(indent, newline) + std::format("{} {}",
    decl->toString(parent, this, indent, newline),
    body->toString(parent, this, indent, newline)
  );
}

llvm::Function * FnDef::generateFunction(codegen::ModuleContext& ctx, PayloadList payload) {
  decl->generateFunction(ctx, {});

  auto meta_fn = ctx.globalContext.getMetaFunction(decl->name->name());

  auto fn = ctx.getFunction(decl->name->name());

  if (!fn) {
    Error(ERROR_INTERNAL_FAILURE, decl->span, "Error generating Function object for '{}'", decl->name->name()).raiseFromNode(this);
  }

  auto di_fn = ctx.globalContext.di_builder->createFunction(
    ctx.currentDIScope(), // or ctx.currentScope().di_scope
    meta_fn->name,
    meta_fn->name,
    ctx.globalContext.getCurrentDIFile(),
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
    logger.debug("Function {} IR:", meta_fn->name);
    logger.print("{}", fn_collector.string());
#endif
    Error(ERROR_LLVM_ERROR, decl->span, "Function '{}' didn't pass validation", decl->name->name())
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

    llvm::DILocalVariable * di_param = ctx.globalContext.di_builder->createParameterVariable(
      di_fn,
      arg_name,
      arg.getArgNo() + 1,
      ctx.globalContext.getCurrentDIFile(),
      span.start().line,
      meta_fn->args[arg_name]->getDIType(ctx),
      true
    );

    ctx.globalContext.di_builder->insertDeclare(
      ctx.getLocalValue(arg_name),
      di_param,
      ctx.globalContext.di_builder->createExpression(),
      span.start().getDILocation(ctx, di_fn),
      ctx.ir_builder->GetInsertBlock()
    );

    ctx.ir_builder->CreateStore(&arg, ctx.getLocalValue(arg_name));
  }

  ctx.globalContext.setCurrentFunction(decl->name->name());

  auto last_val = body->generateValue(ctx, extendPayload(payload, Block::Payload::create(meta_fn->returnType)));

  // Pop function scope
  ctx.popScope();

  if (!body->body.back()->is(AST_RETURN)) {
    if (meta_fn->returnType->isVoid()) {
      ctx.ir_builder->CreateRetVoid();
    } else {
      last_val = castIfNotSame(ctx, last_val, meta_fn->getLLVMReturnType(ctx), body->body.back()->span);
      ctx.ir_builder->CreateRet(last_val);
    }
  }
}

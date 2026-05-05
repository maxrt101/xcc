#include "xcc/ast/fndef.h"
#include "xcc/ast/string.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/util/log.h"

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
  return std::format("{} {}",
    decl->toString(parent, this, indent, newline),
    body->toString(parent, this, indent, newline)
  );
}

llvm::Function * FnDef::generateFunction(codegen::ModuleContext& ctx, PayloadList payload) {
  decl->generateFunction(ctx, {});

  auto meta_fn = ctx.globalContext.getMetaFunction(decl->name->name());

  auto fn = ctx.getFunction(decl->name->name());

  if (!fn) {
    Error(ERROR_INTERNAL_FAILURE, decl->span, "Error generating Function object for '{}'", decl->name->name()).throwException();
  }

  auto basic_block = llvm::BasicBlock::Create(*ctx.llvm.ctx, "entry", fn);

  ctx.ir_builder->SetInsertPoint(basic_block);

  ctx.locals.clear();

  for (auto& arg : fn->args()) {
    auto arg_name = std::string(arg.getName());
    ctx.locals[arg_name] = meta::TypedValue::create(ctx, fn, meta_fn->args[arg_name], arg_name);
    ctx.ir_builder->CreateStore(&arg, ctx.locals[arg_name]->value);
  }

  ctx.globalContext.setCurrentFunction(decl->name->name());

  auto last_val = body->generateValue(ctx, {});

  if (!body->body.back()->is(AST_RETURN)) {
    if (meta_fn->returnType->isVoid()) {
      ctx.ir_builder->CreateRetVoid();
    } else {
      last_val = codegen::castIfNotSame(ctx, last_val, meta_fn->getLLVMReturnType(ctx));
      ctx.ir_builder->CreateRet(last_val);
    }
  }

  if (hasAttribute("section")) {
    auto attr = getAttribute("section");

    attr.validateArgsStrict({AST_EXPR_STRING});

    auto section_name = attr.args[0]->as<ast::String>()->value;

    logger.debug("Placing '{}' in section '{}'", decl->name->name(), section_name);

    fn->setSection(section_name);
  }

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
      .note({}, "{}", std::string(collector.string()))
      .throwException();
  }

  return fn;
}

std::shared_ptr<meta::Type> FnDef::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  return decl->generateType(ctx, payload);
}

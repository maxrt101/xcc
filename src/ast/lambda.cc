#include "xcc/ast/lambda.h"
#include "xcc/util/log.h"
#include "xcc/codegen.h"

using namespace xcc;
using namespace xcc::ast;

static auto& logger = xcc::log::Logger::get("FN", log::Flag::SPLIT_ON_NEWLINE);

struct CaptureInfo {
  std::string                 name;
  std::shared_ptr<meta::Type> type;
  bool                        isPointer;
  std::shared_ptr<Node>       node;
};

uint64_t Lambda::counter = 0;

Lambda::Lambda(
    SourceSpan                                    span,
    LexicalScope                                  scope,
    std::vector<Capture>                          captures,
    std::vector<std::shared_ptr<TypedIdentifier>> args,
    std::shared_ptr<Node>                         return_type,
    std::shared_ptr<Block>                        body,
    bool                                          isVariadic
) : Node(AST_LAMBDA, span, scope),
    captures(std::move(captures)),
    args(std::move(args)),
    return_type(std::move(return_type)),
    body(std::move(body)),
    isVariadic(isVariadic) {}

std::shared_ptr<Lambda> Lambda::create(
    SourceSpan                                    span,
    LexicalScope                                  scope,
    std::vector<Capture>                          captures,
    std::vector<std::shared_ptr<TypedIdentifier>> args,
    std::shared_ptr<Node>                         return_type,
    std::shared_ptr<Block>                        body,
    bool                                          isVariadic
) {
  return std::make_shared<Lambda>(span, scope, std::move(captures), std::move(args), std::move(return_type), std::move(body), isVariadic);
}

std::shared_ptr<Node> Lambda::clone() {
  std::vector<Capture> cloned_captures;

  for (auto& capture : captures) {
    cloned_captures.emplace_back(capture.name->clone(), capture.expr->clone());
  }

  return withAttrs(create(
    span,
    scope,
    cloned_captures,
    cloneVector(args),
    return_type->clone(),
    cast<Block>(body->clone()),
    isVariadic
  ));
}

void Lambda::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  for (auto& node : captures) {
    callVisitor(globalContext, node.name, visitor, ignoreSubtree);
    callVisitor(globalContext, node.expr, visitor, ignoreSubtree);
  }

  for (auto& node : args) {
    callVisitor(globalContext, node, visitor, ignoreSubtree);
  }

  callVisitor(globalContext, return_type, visitor, ignoreSubtree);
  callVisitor(globalContext, body, visitor, ignoreSubtree);
}

std::string Lambda::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(indent, newline) + "fn [";

  for (size_t i = 0; i < captures.size(); ++i) {
    res += captures[i].expr->toString(parent, this, indent, false);

    if (i + 1 < captures.size()) {
      res += ", ";
    }
  }

  res += "] (";

  for (size_t i = 0; i < args.size(); ++i) {
    res += args[i]->toString(parent, this, indent, false);

    if (i + 1 < args.size()) {
      res += ", ";
    }
  }

  if (isVariadic) {
    res += ", ...";
  }

  res += std::format(") -> {} ", return_type->toString(parent, this, indent, false));
  res += body->toString(parent, this, indent, newline);

  return res;
}

llvm::Value * Lambda::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  auto t = generateLambdaType(ctx, payload);

  // LLVM Type for TypeTag::LAMBDA is closure type (tuple of captures values)
  auto closure_type = t->getLLVMType(ctx);

  llvm::Value * closure = ctx.ir_builder->CreateAlloca(closure_type, nullptr, "closure");

  std::vector<CaptureInfo> capture_info;

  std::unordered_map<std::string, SourceSpan> captured_names;

  // Fill up the closure
  for (size_t i = 0; i < captures.size(); ++i) {
    auto& capture = captures[i];

    bool isPointer = capture.expr->is(AST_EXPR_UNARY);

    // Rely on generateLambdaType to raise an error on bad/invalid AST node
    auto id = isPointer ? capture.expr->as<Unary>()->rhs->as<Identifier>() : capture.expr->as<Identifier>();

    auto name = capture.name ? capture.name->as<Identifier>() : id;

    assertRaiseFromNode(!captured_names.contains(name->value),
      Error(ERROR_NON_UNIQUE_CAPTURE_NAME, capture.expr->span, "Captures must have unique names")
        .note(captured_names[name->value], "Previous capture with the same at"), this);

    captured_names[name->value] = capture.expr->span;

    capture_info.emplace_back(name->value, capture.expr->generateType(ctx, payload), isPointer, capture.expr);

    // Closures should only capture local variables + parent function's arguments
    auto val = ctx.getLocalValue(id->value);

    auto * field_ptr = ctx.ir_builder->CreateStructGEP(closure_type, closure, i);

    ctx.ir_builder->CreateStore(
      isPointer
        ? static_cast<llvm::Value*>(val)
        : static_cast<llvm::Value*>(ctx.ir_builder->CreateLoad(val->getType(), val)),
      field_ptr
    );
  }

  std::vector<llvm::Type *> arg_types;
  arg_types.push_back(ctx.ir_builder->getPtrTy()); // Add implicit 0th argument - Closure

  // Generate types for arguments
  for (auto& arg : args) {
    arg_types.push_back(arg->generateType(ctx, payload)->getLLVMType(ctx));
  }

  // Generate function signature
  auto * lambda_signature = llvm::FunctionType::get(t->getReturnType()->getLLVMType(ctx), arg_types, t->isVariadic());

  // All lambda names are prefixed with "lambda$" and a
  // static monotonic counter is used for uniqueness
  std::string lambda_name = "lambda$" + std::to_string(counter++);

  auto * lambda_fn = llvm::Function::Create(
    lambda_signature, llvm::Function::InternalLinkage, lambda_name, ctx.llvm.module.get()
  );

  // Backup current insert block to restore it later
  auto * current_block = ctx.ir_builder->GetInsertBlock();
  auto current_fn = ctx.globalContext.current_function;

  auto di_fn = ctx.di_builder->createFunction(
    ctx.di_compile_unit,
    lambda_name,
    lambda_name,
    ctx.getCurrentDIFile(),
    span.start().line,
    generateType(ctx, payload)->getDISubroutineType(ctx),
    body->span.start().line,
    llvm::DINode::FlagPrototyped,
    llvm::DISubprogram::SPFlagDefinition
  );

  lambda_fn->setSubprogram(di_fn);

  ctx.setDebugLocation(span, di_fn);

  ctx.globalContext.setCurrentFunction(lambda_name);

  ctx.pushScope(span, di_fn);

  auto * entry = llvm::BasicBlock::Create(*ctx.llvm.ctx, "entry", lambda_fn);
  ctx.ir_builder->SetInsertPoint(entry);

  llvm::Value * closure_arg = lambda_fn->getArg(0);
  closure_arg->setName("$closure");

  ctx.addDIParameter(di_fn, "$closure", t, span, 0, closure_arg);

  // Unpack captures values from closure into local scope
  for (size_t i = 0; i < capture_info.size(); ++i) {
    auto& capture = capture_info[i];

    auto * field_ptr = ctx.ir_builder->CreateStructGEP(closure_type, closure_arg, i, capture.name + "_gep");

    ctx.addLocal(capture.name, meta::TypedValue::create(ctx, lambda_fn, capture.node->span, capture.type, capture.name));

    llvm::Value * val = ctx.ir_builder->CreateLoad(ctx.ir_builder->getPtrTy(), field_ptr, capture.name + "_ptr");

    ctx.ir_builder->CreateStore(val, ctx.getLocalValue(capture.name));
  }

  // Unpack arguments
  for (size_t i = 0; i < args.size(); ++i) {
    size_t llvmIdx = i + 1;

    auto name = args[i]->name->value;
    auto type = args[i]->value_type->generateType(ctx, payload);
    auto val = lambda_fn->getArg(llvmIdx);

    ctx.addLocal(name, meta::TypedValue::create(ctx, lambda_fn, args[i]->span, type, name));

    ctx.addDIParameter(di_fn, name, type, args[i]->span, llvmIdx);

    ctx.ir_builder->CreateStore(val, ctx.getLocalValue(name));
  }

  // Generate actual lambda body
  auto val = body->generateValue(ctx, extendPayload(excludePayload(payload, AST_BLOCK), Block::Payload::create(t->getReturnType(), true)));

  ctx.popScope(isOrIsLastInBlock(body->body.back(), AST_RETURN));

  // Create terminator
  if (!body->body.back()->is(AST_RETURN)) {
    if (t->getReturnType()->isVoid()) {
      ctx.ir_builder->CreateRetVoid();
    } else {
      val = castIfNotSame(ctx, val, t->getReturnType()->getLLVMType(ctx), body->body.back()->span);
      ctx.ir_builder->CreateRet(val);
    }
  }

  util::RawStreamCollector collector;
  if (llvm::verifyFunction(*lambda_fn, collector.stream())) {
#if USE_PRINT_LLVM_IR_ON_VERIFY_FAIL
    util::RawStreamCollector fn_collector;
    lambda_fn->print(*fn_collector.stream());
    logger.debug("Function '{}' IR:", lambda_name);
    logger.print("{}", fn_collector.string());
#endif
    Error(ERROR_LLVM_ERROR, span, "Function '{}' didn't pass validation", lambda_name)
      .note("{}", std::string(collector.string()))
      .raiseFromNode(this);
  }

  // Restore block
  ctx.ir_builder->SetInsertPoint(current_block);
  ctx.globalContext.setCurrentFunction(current_fn);

  ctx.setDebugLocation(span);

  // Assemble fat pointer (function address + closure)
  auto fat_ptr_type = t->getLambdaFunctionType()->getLLVMType(ctx);

  llvm::Value * fat_ptr = llvm::PoisonValue::get(fat_ptr_type);

  fat_ptr = ctx.ir_builder->CreateInsertValue(fat_ptr, lambda_fn, 0, "lambda_ptr");
  fat_ptr = ctx.ir_builder->CreateInsertValue(fat_ptr, closure,   1, "closure_ptr");

  return fat_ptr;
}

std::shared_ptr<meta::Type> Lambda::generateType(codegen::ModuleContext &ctx, PayloadList payload) {
  return generateLambdaType(ctx, payload)->getLambdaFunctionType();
}

std::shared_ptr<meta::Type> Lambda::generateLambdaType(codegen::ModuleContext &ctx, PayloadList payload) {
  std::vector<std::shared_ptr<meta::Type>> captures, args;

  for (auto& capture : this->captures) {
    if (capture.name) {
      assertRaiseFromNode(capture.name->is(AST_EXPR_IDENTIFIER),
        Error(ERROR_CAPTURE_NAME_EXPECTED_IDENTIFIER, capture.name->span), this);
    }

    assertRaiseFromNode(capture.expr->isAnyOf(AST_EXPR_IDENTIFIER, AST_EXPR_UNARY),
      Error(ERROR_LAMBDA_BAD_CAPTURE_EXPR, capture.expr->span)
      .note("Only identifier or unary '&' operation are allowed in lambda capture"), this);

    if (capture.expr->is(AST_EXPR_UNARY)) {
      assertRaiseFromNode(capture.expr->as<Unary>()->operation.is(TOKEN_AMP),
        Error(ERROR_LAMBDA_BAD_CAPTURE_EXPR, capture.expr->span)
        .note("Only '&' is allowed in unary expression in lambda capture"), this);

      assertRaiseFromNode(capture.expr->as<Unary>()->rhs->is(AST_EXPR_IDENTIFIER),
        Error(ERROR_LAMBDA_BAD_CAPTURE_EXPR, capture.expr->span)
        .note("RHS in unary expression must be an identifier for lambda capture"), this);
    }

    captures.push_back(capture.expr->generateType(ctx, payload));
  }

  for (auto& arg : this->args) {
    args.push_back(arg->generateType(ctx, payload));
  }

  return meta::Type::createLambda(
    meta::Type::createFunction(
      return_type->generateType(ctx, payload),
      args,
      isVariadic
    ),
    captures
  );
}

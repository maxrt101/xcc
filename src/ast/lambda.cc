#include "xcc/ast/lambda.h"

#include "xcc/codegen.h"

using namespace xcc;
using namespace xcc::ast;

struct CaptureInfo {
  std::string                 name;
  std::shared_ptr<meta::Type> type;
  bool                        isPointer;
  std::shared_ptr<Node>       node;
};

uint64_t Lambda::counter = 0;

Lambda::Lambda(
    SourceSpan                                    span,
    NodeList                                      captures,
    std::vector<std::shared_ptr<TypedIdentifier>> args,
    std::shared_ptr<Node>                         return_type,
    std::shared_ptr<Block>                        body,
    bool                                          isVariadic
) : Node(AST_LAMBDA, span),
    captures(std::move(captures)),
    args(std::move(args)),
    return_type(std::move(return_type)),
    body(std::move(body)),
    isVariadic(isVariadic) {}

std::shared_ptr<Lambda> Lambda::create(
    SourceSpan                                    span,
    NodeList                                      captures,
    std::vector<std::shared_ptr<TypedIdentifier>> args,
    std::shared_ptr<Node>                         return_type,
    std::shared_ptr<Block>                        body,
    bool                                          isVariadic
) {
  return std::make_shared<Lambda>(span, std::move(captures), std::move(args), std::move(return_type), std::move(body), isVariadic);
}

std::shared_ptr<Node> Lambda::clone() {
  return withAttrs(create(
    span,
    cloneVector(captures),
    cloneVector(args),
    return_type->clone(),
    cast<Block>(body->clone()),
    isVariadic
  ));
}

void Lambda::visit(std::unique_ptr<codegen::GlobalContext>& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  for (auto& node : captures) {
    callVisitor(globalContext, node, visitor, ignoreSubtree);
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
    res += captures[i]->toString(parent, this, indent, false);

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

  res += std::format(") -> {}", return_type->toString(parent, this, indent, false));
  res += body->toString(parent, this, indent + 1, newline);

  return res;
}

llvm::Value * Lambda::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  auto t = generateLambdaType(ctx, payload);

  auto closure_type = t->getLLVMType(ctx);

  llvm::Value * closure = ctx.ir_builder->CreateAlloca(closure_type, nullptr, "closure");

  std::vector<CaptureInfo> capture_info;

  for (size_t i = 0; i < captures.size(); ++i) {
    auto& capture = captures[i];

    bool isPointer = capture->is(AST_EXPR_UNARY);

    // Rely on generateLambdaType to raise an error on bad/invalid AST node
    auto id = isPointer ? capture->as<Unary>()->rhs->as<Identifier>() : capture->as<Identifier>();

    capture_info.emplace_back(id->value, capture->generateType(ctx, payload), isPointer, capture);

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
  arg_types.push_back(ctx.ir_builder->getPtrTy()); // Closure

  for (auto& arg : args) {
    arg_types.push_back(arg->generateType(ctx, payload)->getLLVMType(ctx));
  }

  auto * lambda_signature = llvm::FunctionType::get(t->getReturnType()->getLLVMType(ctx), arg_types, t->isVariadic());

  std::string lambda_name = "lambda$" + std::to_string(counter++);

  auto * lambda_fn = llvm::Function::Create(
    lambda_signature, llvm::Function::InternalLinkage,lambda_name, ctx.llvm.module.get()
  );

  auto * current_block = ctx.ir_builder->GetInsertBlock();

  auto di_fn = ctx.globalContext.di_builder->createFunction(
    ctx.currentDIScope(),
    lambda_name,
    lambda_name,
    ctx.globalContext.getCurrentDIFile(),
    span.start().line,
    generateType(ctx, payload)->getDISubroutineType(ctx),
    body->span.start().line,
    llvm::DINode::FlagPrototyped,
    llvm::DISubprogram::SPFlagDefinition
  );

  lambda_fn->setSubprogram(di_fn);

  ctx.setDebugLocation(span, di_fn);

  ctx.pushScope(span);

  auto * worker_entry = llvm::BasicBlock::Create(*ctx.llvm.ctx, "entry", lambda_fn);
  ctx.ir_builder->SetInsertPoint(worker_entry);

  llvm::Value * closure_arg = lambda_fn->getArg(0);
  closure_arg->setName("$closure");

  ctx.addDIParameter(di_fn, "$closure", t, span, 0, closure_arg);

  for (size_t i = 0; i < capture_info.size(); ++i) {
    auto& capture = capture_info[i];

    auto * field_ptr = ctx.ir_builder->CreateStructGEP(closure_type, closure_arg, i, capture.name + "_gep");

    ctx.addLocal(capture.name, meta::TypedValue::create(ctx, lambda_fn, capture.node->span, capture.type, capture.name));

    llvm::Value * val;

    if (capture.isPointer) {
      val = ctx.ir_builder->CreateLoad(ctx.ir_builder->getPtrTy(), field_ptr, capture.name + "_ptr");
    } else {
      val = field_ptr;
    }

    ctx.ir_builder->CreateStore(val, ctx.getLocalValue(capture.name));
  }

  for (size_t i = 0; i < args.size(); ++i) {
    size_t llvmIdx = i + 1;

    auto name = args[i]->name->value;
    auto type = args[i]->value_type->generateType(ctx, payload);
    auto val = lambda_fn->getArg(llvmIdx);

    ctx.addLocal(name, meta::TypedValue::create(ctx, lambda_fn, args[i]->span, type, name));

    ctx.addDIParameter(di_fn, name, type, args[i]->span, llvmIdx);

    ctx.ir_builder->CreateStore(val, ctx.getLocalValue(name));
  }

  auto val = body->generateValue(ctx, payload);

  ctx.popScope();

  if (!body->body.back()->is(AST_RETURN)) {
    if (t->getReturnType()->isVoid()) {
      ctx.ir_builder->CreateRetVoid();
    } else {
      val = castIfNotSame(ctx, val, t->getReturnType()->getLLVMType(ctx), body->body.back()->span);
      ctx.ir_builder->CreateRet(val);
    }
  }

  ctx.ir_builder->SetInsertPoint(current_block);

  ctx.setDebugLocation(span);

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
    assertRaiseFromNode(capture->isAnyOf(AST_EXPR_IDENTIFIER, AST_EXPR_UNARY),
      Error(ERROR_LAMBDA_BAD_CAPTURE_EXPR, capture->span)
      .note("Only identifier or unary '&' operation are allowed in lambda capture"), this);

    if (capture->is(AST_EXPR_UNARY)) {
      assertRaiseFromNode(capture->as<Unary>()->operation.is(TOKEN_AMP),
        Error(ERROR_LAMBDA_BAD_CAPTURE_EXPR, capture->span)
        .note("Only '&' is allowed in unary expression in lambda capture"), this);

      assertRaiseFromNode(capture->as<Unary>()->rhs->is(AST_EXPR_IDENTIFIER),
        Error(ERROR_LAMBDA_BAD_CAPTURE_EXPR, capture->span)
        .note("RHS in unary expression must be an identifier for lambda capture"), this);
    }

    captures.push_back(capture->generateType(ctx, payload));
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

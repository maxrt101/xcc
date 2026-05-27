#include "xcc/ast/call.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/ast/member.h"

using namespace xcc::ast;

Call::Call(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> callee, NodeList args)
    : Node(AST_EXPR_CALL, span, scope), callee(std::move(callee)), args(std::move(args)) {}

std::shared_ptr<Call> Call::create(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> name, NodeList args) {
  return std::make_shared<Call>(span, scope, std::move(name), std::move(args));
}

std::shared_ptr<Node> Call::clone() {
  return withAttrs(create(span, scope, callee->clone(), cloneVector(args)));
}

void Call::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(globalContext, callee, visitor, ignoreSubtree);

  for (auto& node : args) {
    callVisitor(globalContext, node, visitor, ignoreSubtree);
  }
}

std::string Call::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(0, false) + callee->toString(parent, this, indent, newline) + "(";

  for (size_t i = 0; i < args.size(); ++i) {
    res += args[i]->toString(parent, this, indent, false);

    if (i + 1 < args.size()) {
      res += ", ";
    }
  }

  return res + ")";
}

llvm::Value * Call::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  auto info = getCalleeInfo(ctx, payload, true);

  llvm::FunctionType * signature = info.metaType->getLLVMFunctionType(ctx);

  std::vector<llvm::Value *> arg_vals;

  bool isDirectCall = llvm::isa<llvm::Function>(info.calleePtr);

  if (info.isMember) {
    auto selfNode = callee->as<MemberAccess>()->lhs;
    auto selfType = selfNode->generateType(ctx, payload);

    llvm::Value * self_ptr = selfNode->generateValueWithoutLoad(ctx, payload);

    if (!self_ptr->getType()->isPointerTy()) {
      auto * alloca = ctx.ir_builder->CreateAlloca(
          selfType->getLLVMType(ctx),
          nullptr,
          "self_alloca_tmp"
      );

      ctx.ir_builder->CreateStore(self_ptr, alloca);

      self_ptr = alloca;
    }

    arg_vals.push_back(self_ptr);
  }

  size_t expectedArgs = signature->getNumParams();
  size_t providedArgs = args.size() + (info.isMember ? 1 : 0);

  bool isVariadic = info.metaType->isVariadic();

  if (!isVariadic && expectedArgs != providedArgs) {
    auto err = Error(ERROR_FN_CALL_ARG_COUNT_MISMATCH, span,
        "function '{}', expected: {}, got: {}",
        info.fnName, expectedArgs, providedArgs);

    if (info.isMember) {
      err = err.note("Looks like '{}' is a method, maybe it is a static one and doesn't expect 'self'? Try to access it via '::'", info.fnName);
    }

    err.raiseFromNode(this);
  }

  for (size_t i = 0; i < args.size(); ++i) {
    size_t llvmParamIdx = info.isMember ? i + 1 : i;

    auto type = args[i]->generateType(ctx, payload);
    auto val  = args[i]->generateValue(ctx, payload);;

    if (!val) {
      Error(ERROR_INTERNAL_FAILURE, args[i]->span, "Failed to generate function call argument #{}", i).raiseFromNode(this);
    }

    if (i < signature->getNumParams()) {
      // Positional argument
      val = castIfNotSame(ctx, val, signature->getParamType(llvmParamIdx), args[i]->span);
    } else {
      // Variadic argument
      llvm::Type * argType = val->getType();

      if (argType->isIntegerTy() && argType->getIntegerBitWidth() < 32) {
        // ABI Rule: Promote all types, smaller than 32 bits into i32/u32
        // Bool is a special case - as calling SExt on it makes it flip around to -1
        if (type->isUnsigned() || type->isBool()) {
          val = ctx.ir_builder->CreateZExt(val, ctx.ir_builder->getInt32Ty());
        } else {
          val = ctx.ir_builder->CreateSExt(val, ctx.ir_builder->getInt32Ty());
        }
      } else if (argType->isFloatTy()) {
        // ABI Rule: Promote float (32-bit) to double (64-bit)
        val = ctx.ir_builder->CreateFPExt(val, ctx.ir_builder->getDoubleTy());
      }
    }

    // TODO: Move semantics
    // if (type->isStruct() && type->isDrop()) {
    //   if (isOrIsLastInBlock(args[i], AST_EXPR_CALL) || isOrIsLastInBlock(args[i], AST_INIT)) {
    //     ctx.currentScope().raii.forgetLastTemporary();
    //   } else if (isOrIsLastInBlock(args[i], AST_EXPR_CALL)) {
    //     ctx.currentScope().raii.forget(getOrGetLastInBlock(args[i], AST_EXPR_IDENTIFIER)->as<Identifier>()->value);
    //   }
    // }

    arg_vals.push_back(val);
  }

  if (isDirectCall) {
    /* Direct call to a global function. info.calleePtr is llvm::Function* - a raw pointer to function
     * Everything is simple in this case */

    if (signature->getReturnType()->isVoidTy()) {
      // Don't name a temporary return value, as there is no value
      return ctx.ir_builder->CreateCall(signature, info.calleePtr, arg_vals);
    }

    auto ret_type = info.metaType->getReturnType();

    return checkRAII(ctx, ctx.ir_builder->CreateCall(signature, info.calleePtr, arg_vals, "calltmp"), ret_type);
  }

  /* Call by function pointer, which could either be a lambda or a global function pointer
   * In both cases info.calleePtr is a fat pointer with {callee, closure} structure
   */

  llvm::Value * fat_ptr = info.calleePtr;

  llvm::Value * code_ptr    = ctx.ir_builder->CreateExtractValue(fat_ptr, 0, "lambda_fn_ptr");
  llvm::Value * closure_ptr = ctx.ir_builder->CreateExtractValue(fat_ptr, 1, "lambda_closure_ptr");

  std::vector<llvm::Value *> fat_arg_vals = {closure_ptr};
  fat_arg_vals.insert(fat_arg_vals.end(), arg_vals.begin(), arg_vals.end());

  std::vector<llvm::Type *> param_types;
  param_types.push_back(ctx.ir_builder->getPtrTy());

  for (auto& param_type : info.metaType->getArgumentTypes()) {
    param_types.push_back(param_type->getLLVMType(ctx));
  }

  auto ret_type = info.metaType->getReturnType();

  llvm::Type * ret_llvm_type = info.metaType->getReturnType()->getLLVMType(ctx);
  llvm::FunctionType * worker_signature = llvm::FunctionType::get(
    ret_llvm_type,
    param_types,
    info.metaType->isVariadic()
  );

  if (ret_llvm_type->isVoidTy()) {
    return ctx.ir_builder->CreateCall(worker_signature, code_ptr, fat_arg_vals);
  }

  return checkRAII(ctx, ctx.ir_builder->CreateCall(worker_signature, code_ptr, fat_arg_vals, "calltmp"), ret_type);
}

std::shared_ptr<xcc::meta::Type> Call::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  auto info = getCalleeInfo(ctx, payload, false);
  return info.metaType->getReturnType();
}

Call::CalleeInfo Call::getCalleeInfo(codegen::ModuleContext& ctx, PayloadList payload, bool generateCallee) {
  CalleeInfo info;

  if (auto * ident = dynamic_cast<Identifier*>(callee.get())) {
    getCalleeInfoForFunctionCall(ctx, payload, info, ident, generateCallee);
  } else if (auto * memberAccess = dynamic_cast<MemberAccess*>(callee.get())) {
    getCalleeInfoForMethodCall(ctx, payload, info, memberAccess);
  } else {
    // Complex expressions
    info.metaType = callee->generateType(ctx, payload);

    if (generateCallee) {
      info.calleePtr = callee->generateValue(ctx, payload);
    }
  }

  if (!info.metaType || (!info.metaType->isFunction() && !info.metaType->isLambda())) {
    Error(ERROR_EXPR_NOT_CALLABLE, callee->span, "{} is not callable", typeToHumanReadableString(callee->type)).raiseFromNode(this);
  }

  return info;
}

void Call::getCalleeInfoForFunctionCall(codegen::ModuleContext& ctx, PayloadList payload, CalleeInfo& info, Identifier * ident, bool generateCallee) {
  info.fnName = ctx.globalContext.aliased(ident->name());

  if (auto * directFn = ctx.getFunction(info.fnName)) {
    // Direct function call

    info.metaType  = ctx.globalContext.getMetaFunctionType(info.fnName);
    info.calleePtr = directFn;

    assertRaiseFromNode(info.metaType.get(), Error(ERROR_UNKNOWN_FUNCTION, ident->span, "'{}'", info.fnName), this);
  } else {
    // Function pointer call
    info.metaType = ident->generateType(ctx, payload);

    if (generateCallee) {
      info.calleePtr = ident->generateValue(ctx, payload);
    }
  }
}

void Call::getCalleeInfoForMethodCall(codegen::ModuleContext& ctx, PayloadList payload, CalleeInfo& info, MemberAccess * memberAccess) {
  // Method call
  info.isMember = true;
  auto caleeType = memberAccess->lhs->generateType(ctx, payload);

  // For by-pointer method retrieval (`self->func()`) calleeType with be `T*`, instead of `T`,
  // which will mess up name mangling. If `T` is enum or struct - replace calleeType with it
  if (caleeType->isPointer()) {
    auto pointed = caleeType->getPointedType();

    if (pointed->isStruct() || pointed->isEnum()) {
      caleeType = pointed;
    }
  }

  info.fnName = caleeType->getName() + "_" + memberAccess->rhs->value;

  auto * directFn = ctx.getFunction(info.fnName);

  if (!directFn) {
    Error(ERROR_UNKNOWN_METHOD, callee->span, "'{}'", info.fnName).raiseFromNode(this);
  }

  info.calleePtr = directFn;
  info.metaType  = ctx.globalContext.getMetaFunctionType(info.fnName);

  assertRaiseFromNode(info.metaType.get(), Error(ERROR_UNKNOWN_FUNCTION, {}, "'{}'", info.fnName), this);
}

llvm::Value * Call::checkRAII(codegen::ModuleContext& ctx, llvm::Value * ret_val, std::shared_ptr<meta::Type> ret_type) {
  // If return value is a struct and has a drop method
  // create a temporary alloca, store result value there
  // and record the alloca into RAII context
  if (ret_type->isStruct() && ret_type->isDrop()) {
    auto * alloca = ctx.ir_builder->CreateAlloca(ret_val->getType(), nullptr, "temp_ret");

    ctx.ir_builder->CreateStore(ret_val, alloca);

    ctx.currentScope().raii.addTemporary(alloca, ret_type);

    return ctx.ir_builder->CreateLoad(ret_val->getType(), alloca);
  }

  return ret_val;
}

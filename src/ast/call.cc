#include "xcc/ast/call.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/ast/member.h"

using namespace xcc::ast;

Call::Call(std::shared_ptr<Node> callee, std::vector<std::shared_ptr<Node>> args)
    : Node(AST_EXPR_CALL), callee(std::move(callee)), args(std::move(args)) {}

std::shared_ptr<Call> Call::create(std::shared_ptr<Node> name, std::vector<std::shared_ptr<Node>> args) {
  return std::make_shared<Call>(std::move(name), std::move(args));
}

llvm::Value * Call::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  auto info = getCalleeInfo(ctx, payload, true);

  llvm::FunctionType * signature = info.metaType->getLLVMFunctionType(ctx);

  std::vector<llvm::Value *> arg_vals;

  if (info.isMember) {
    auto selfNode = callee->as<MemberAccess>()->lhs;
    arg_vals.push_back(selfNode->generateValueWithoutLoad(ctx, {}));
  }

  size_t expectedArgs = signature->getNumParams();
  size_t providedArgs = args.size() + (info.isMember ? 1 : 0);

  bool isVariadic = info.metaType->isVariadic();

  if (!isVariadic && expectedArgs != providedArgs) {
    throw CodegenException(std::format(
        "Argument mismatch (function: '{}', expected: {}, got: {})",
        info.fnName, expectedArgs, providedArgs));
  }

  for (size_t i = 0; i < args.size(); ++i) {
    size_t llvmParamIdx = info.isMember ? i + 1 : i;

    /* If member and first arg - it's 'self', which is allways a pointer, so should
     * generate ValueWithoutLoad, otherwise - just generate value */
    auto val = (info.isMember && i == 0)
      ? args[i]->generateValueWithoutLoad(ctx, {})
      : args[i]->generateValue(ctx, {});

    if (!val) {
      throw CodegenException("Failed to generate function call arguments");
    }

    if (i < signature->getNumParams()) {
      val = codegen::castIfNotSame(ctx, val, signature->getParamType(llvmParamIdx));
    }

    arg_vals.push_back(val);
  }

  if (signature->getReturnType()->isVoidTy()) {
    // Don't name a temporary return value, as there is no value
    return ctx.ir_builder->CreateCall(signature, info.calleePtr, arg_vals);
  }

  return ctx.ir_builder->CreateCall(signature, info.calleePtr, arg_vals, "calltmp");
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
    info.metaType = callee->generateType(ctx, {});

    if (generateCallee) {
      info.calleePtr = callee->generateValue(ctx, {});
    }
  }

  if (!info.metaType || !info.metaType->isFunction()) {
    throw CodegenException("Expression is not callable");
  }

  return info;
}

void Call::getCalleeInfoForFunctionCall(codegen::ModuleContext& ctx, PayloadList payload, CalleeInfo& info, Identifier* ident, bool generateCallee) {
  info.fnName = ident->name();

  if (auto * directFn = ctx.getFunction(info.fnName)) {
    // Direct function call
    auto meta_fn   = ctx.globalContext.getMetaFunction(info.fnName);
    info.metaType  = meta_fn->decl->generateType(ctx, payload);
    info.calleePtr = directFn;
  } else {
    // Function pointer call
    info.metaType  = ident->generateType(ctx, payload);

    if (generateCallee) {
      info.calleePtr = ident->generateValue(ctx, payload);
    }
  }
}

void Call::getCalleeInfoForMethodCall(codegen::ModuleContext& ctx, PayloadList payload, CalleeInfo& info, MemberAccess * memberAccess) {
  // Method call
  info.isMember = true;
  auto structType = memberAccess->lhs->generateType(ctx, payload);

  info.fnName = structType->getName() + "_" + memberAccess->rhs->value;

  auto * directFn = ctx.getFunction(info.fnName);

  if (!directFn) {
    throw CodegenException("Unknown method: " + info.fnName);
  }

  auto meta_fn   = ctx.globalContext.getMetaFunction(info.fnName);
  info.calleePtr = directFn;
  info.metaType  = meta_fn->decl->generateType(ctx, payload);
}

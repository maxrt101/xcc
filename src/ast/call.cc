#include "xcc/ast/call.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/ast/member.h"

using namespace xcc::ast;

Call::Call(SourceSpan span, std::shared_ptr<Node> callee, NodeList args)
    : Node(AST_EXPR_CALL, span), callee(std::move(callee)), args(std::move(args)) {}

std::shared_ptr<Call> Call::create(SourceSpan span, std::shared_ptr<Node> name, NodeList args) {
  return std::make_shared<Call>(span, std::move(name), std::move(args));
}

std::shared_ptr<Node> Call::clone() {
  return withAttrs(create(span, callee->clone(), cloneVector(args)));
}

void Call::visit(std::unique_ptr<codegen::GlobalContext>& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
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
    Error(ERROR_FN_CALL_ARG_COUNT_MISMATCH, span,
        "Argument mismatch (function: '{}', expected: {}, got: {})",
        info.fnName, expectedArgs, providedArgs).raiseFromNode(this);
  }

  for (size_t i = 0; i < args.size(); ++i) {
    size_t llvmParamIdx = info.isMember ? i + 1 : i;

    /* If member and first arg - it's 'self', which is allways a pointer, so should
     * generate ValueWithoutLoad, otherwise - just generate value */
    auto val = (info.isMember && i == 0)
      ? args[i]->generateValueWithoutLoad(ctx, payload)
      : args[i]->generateValue(ctx, payload);

    if (!val) {
      Error(ERROR_INTERNAL_FAILURE, args[i]->span, "Failed to generate function call argument #{}", i).raiseFromNode(this);
    }

    if (i < signature->getNumParams()) {
      // Positional argument
      val = castIfNotSame(ctx, val, signature->getParamType(llvmParamIdx), args[i]->span);
    } else {
      // Variadic argument
      llvm::Type* argType = val->getType();

      if (argType->isIntegerTy() && argType->getIntegerBitWidth() < 32) {
        // ABI Rule: Promote i1, i8, i16 to i32
        val = ctx.ir_builder->CreateSExt(val, ctx.ir_builder->getInt32Ty());
      } else if (argType->isFloatTy()) {
        // ABI Rule: Promote float (32-bit) to double (64-bit)
        val = ctx.ir_builder->CreateFPExt(val, ctx.ir_builder->getDoubleTy());
      }
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
    info.metaType = callee->generateType(ctx, payload);

    if (generateCallee) {
      info.calleePtr = callee->generateValue(ctx, payload);
    }
  }

  if (!info.metaType || !info.metaType->isFunction()) {
    Error(ERROR_EXPR_NOT_CALLABLE, callee->span, "{} of type is not callable", typeToHumanReadableString(callee->type)).raiseFromNode(this);
  }

  return info;
}

void Call::getCalleeInfoForFunctionCall(codegen::ModuleContext& ctx, PayloadList payload, CalleeInfo& info, Identifier* ident, bool generateCallee) {
  info.fnName = ctx.globalContext.aliased(ident->name());

  if (auto * directFn = ctx.getFunction(info.fnName)) {
    // Direct function call
    auto meta_fn = ctx.globalContext.getMetaFunction(info.fnName);

    assertRaiseFromNode(meta_fn.get(), Error(ERROR_UNKNOWN_FUNCTION, ident->span), this);

    info.metaType  = meta_fn->decl->generateType(ctx, payload);
    info.calleePtr = directFn;
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
  auto caleeeType = memberAccess->lhs->generateType(ctx, payload);

  info.fnName = caleeeType->getName() + "_" + memberAccess->rhs->value;

  auto * directFn = ctx.getFunction(info.fnName);

  if (!directFn) {
    Error(ERROR_UNKNOWN_METHOD, callee->span, "'{}'", info.fnName).raiseFromNode(this);
  }

  auto meta_fn   = ctx.globalContext.getMetaFunction(info.fnName);
  info.calleePtr = directFn;
  info.metaType  = meta_fn->decl->generateType(ctx, payload);
}

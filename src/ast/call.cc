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
  std::string fnName;
  bool isMember;

  getInfoFromCallee(ctx, &fnName, &isMember);

  llvm::Function * fn = throwIfNull(ctx.getFunction(fnName), CodegenException("Unknown function to call ('" + fnName + "')"));

  /* If member - add implicit 'self' value, which is an LHS of callee MemberAccess tree */
  if (isMember) {
    args.insert(args.begin(), callee->as<MemberAccess>()->lhs);
  }

  auto meta_fn = ctx.globalContext.getMetaFunction(fnName);

  if (!meta_fn->decl->isVariadic && fn->arg_size() != args.size()) {
    throw CodegenException("Argument mismatch (function: '" + fnName + "', expected: " + std::to_string(fn->arg_size()) + ", got: " + std::to_string(args.size()) + ")");
  }

  std::vector<llvm::Value *> arg_vals;

  for (size_t i = 0; i < args.size(); ++i) {
    /* If member and first arg - it's 'self', which is allways a pointer, so should
     * generate ValueWithoutLoad, otherwise - just generate value */
    auto val = (isMember && i == 0)
      ? args[i]->generateValueWithoutLoad(ctx, {})
      : args[i]->generateValue(ctx, {});

    if (!val) {
      throw CodegenException("Failed to generate function call arguments");
    }

    if (!meta_fn->decl->isVariadic) {
      val = codegen::castIfNotSame(ctx, val, meta_fn->args[i]->getLLVMType(ctx));
    }

    arg_vals.push_back(val);
  }

  if (meta_fn->returnType->isVoid()) {
    // Don't name a temporary return value, as there is no value
    return ctx.ir_builder->CreateCall(fn, arg_vals);
  }

  return ctx.ir_builder->CreateCall(fn, arg_vals, "calltmp");
}

std::shared_ptr<xcc::meta::Type> Call::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  std::string fnName;

  getInfoFromCallee(ctx, &fnName, nullptr);

  auto meta_fn = ctx.globalContext.getMetaFunction(fnName);

  return meta_fn->returnType;
}

void Call::getInfoFromCallee(codegen::ModuleContext& ctx, std::string * name, bool * isMember) const {
  switch (callee->type) {
    /* Plain function call - callee is an identifier */
    case AST_EXPR_IDENTIFIER: {
      if (name)     *name = callee->as<Identifier>()->value;
      if (isMember) *isMember = false;
      break;
    }

    /* Member function call (method) - callee is a MemberAccess tree */
    case AST_EXPR_MEMBER_ACCESS: {
      auto memberAccess = callee->as<MemberAccess>();
      /* Get type of the rest of the MemberAccess tree (which should evaluate to some struct type) and
       * add rhs - member name (which is an identifier) */
      if (name)     *name = memberAccess->lhs->generateType(ctx, {})->getName() + "_" + memberAccess->rhs->value;
      if (isMember) *isMember = true;
      break;
    }

    default:
      throw CodegenException("Can't retrieve function name (invalid callee type " + Node::typeToString(callee->type) + ")");
  }
}

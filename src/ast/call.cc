#include "xcc/ast/call.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;

Call::Call(std::shared_ptr<Node> name, std::vector<std::shared_ptr<Node>> args)
    : Node(AST_EXPR_CALL), name(std::move(name)), args(std::move(args)) {}

std::shared_ptr<Call> Call::create(std::shared_ptr<Node> name, std::vector<std::shared_ptr<Node>> args) {
  return std::make_shared<Call>(std::move(name), std::move(args));
}

llvm::Value * Call::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  std::string fn_name;

  switch (name->type) {
    case AST_EXPR_IDENTIFIER:
      fn_name = name->as<Identifier>()->value;
      break;

    default:
      throw CodegenException("Can't retrieve function name (call code generation)");
  }

  llvm::Function * fn = ctx.getFunction(fn_name);
  auto meta_fn = ctx.globalContext.getMetaFunction(fn_name);

  if (!fn) {
    throw CodegenException("Unknown function to call ('" + fn_name + "')");
  }

  if (!meta_fn->decl->isVariadic && fn->arg_size() != args.size()) {
    throw CodegenException("Argument mismatch (function: '" + fn_name + "', expected: " + std::to_string(fn->arg_size()) + ", got: " + std::to_string(args.size()) + ")");
  }

  std::vector<llvm::Value *> arg_vals;

  for (size_t i = 0; i < args.size(); ++i) {
    auto val = args[i]->generateValue(ctx, {});

    if (!val) {
      throw CodegenException("Failed to generate function call arguments");
    }

    if (!meta_fn->decl->isVariadic) {
      val = codegen::castIfNotSame(ctx, val, meta_fn->args[i]->getLLVMType(ctx));
    }

    arg_vals.push_back(val);
  }

  return ctx.ir_builder->CreateCall(fn, arg_vals, "calltmp");
}

std::shared_ptr<xcc::meta::Type> Call::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
    std::string fn_name;

  switch (name->type) {
    case AST_EXPR_IDENTIFIER:
      fn_name = name->as<Identifier>()->value;
      break;

    default:
      throw CodegenException("Can't retrieve function name (call code generation)");
  }

  auto meta_fn = ctx.globalContext.getMetaFunction(fn_name);

  return meta_fn->returnType;
}

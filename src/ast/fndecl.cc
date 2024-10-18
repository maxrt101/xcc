#include "xcc/ast/fndecl.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc;
using namespace xcc::ast;

FnDecl::FnDecl(std::shared_ptr<Identifier> name, std::shared_ptr<Type> return_type, std::vector<std::shared_ptr<TypedIdentifier>> args, bool isExtern)
  : Node(AST_FUNCTION_DECL), name(std::move(name)), return_type(std::move(return_type)), args(std::move(args)), isExtern(isExtern) {}

std::shared_ptr<FnDecl> FnDecl::create(std::shared_ptr<Identifier> name, std::shared_ptr<Type> return_type, std::vector<std::shared_ptr<TypedIdentifier>> args, bool isExtern) {
  return std::make_shared<FnDecl>(std::move(name), std::move(return_type), std::move(args), isExtern);
}

llvm::Function * FnDecl::generateFunction(codegen::ModuleContext& ctx) {
  std::string fn_name = name->value;

  OrderedMap<std::string, std::shared_ptr<xcc::meta::Type>> arg_meta_types;

  for (auto& arg : args) {
    if (arg->is(AST_EXPR_TYPED_IDENTIFIER)) {
      arg_meta_types[arg->name->value] = arg->generateType(ctx);
    } else {
      throw CodegenException("Unexpected node discovered in '" + fn_name + "' functions argument ('" + Node::typeToString(arg->type) + "')");
    }
  }

  auto return_meta_type = return_type->generateType(ctx);

  auto llvm_fn_type = llvm::FunctionType::get(return_meta_type->getLLVMType(ctx), meta::Function::typesFromMetaArgs(ctx, arg_meta_types), false);
  auto llvm_fn = llvm::Function::Create(llvm_fn_type, isExtern ? llvm::Function::ExternalLinkage : llvm::Function::CommonLinkage, fn_name, ctx.llvm.module.get());

  size_t arg_idx = 0;
  for (auto& arg : llvm_fn->args()) {
    arg.setName(args[arg_idx++]->name->value);
  }

  auto fn = meta::Function::create(
      fn_name,
      return_meta_type,
      arg_meta_types,
      shared_from_this()
  );

  ctx.globalContext.addFunction(fn_name, fn);

  return llvm_fn;
}
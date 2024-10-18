#include "xcc/meta/function.h"

using namespace xcc::meta;

Function::Function(std::string name, std::shared_ptr<Type> returnType, OrderedMap<std::string, std::shared_ptr<Type>> args, std::shared_ptr<ast::FnDecl> decl)
  : name(std::move(name)), returnType(std::move(returnType)), args(std::move(args)), decl(std::move(decl)) {}

std::shared_ptr<Function> Function::create(std::string name, std::shared_ptr<Type> returnType, OrderedMap<std::string, std::shared_ptr<Type>> args, std::shared_ptr<ast::FnDecl> decl) {
  return std::make_shared<Function>(std::move(name), std::move(returnType), std::move(args), std::move(decl));
}

llvm::Type * Function::getLLVMReturnType(codegen::ModuleContext& ctx) {
  return returnType->getLLVMType(ctx);
}

std::vector<llvm::Type *> Function::getLLVMArgTypes(codegen::ModuleContext& ctx) {
  return Function::typesFromMetaArgs(ctx, args);
}

std::string Function::toString() const {
  std::string result = "fn " + name + "(";

  for (auto& arg : args) {
    result += arg + ": " + args[arg]->toString();
    if (arg != args.back()) {
      result += ", ";
    }
  }

  return result + "): " + returnType->toString();
}

std::vector<llvm::Type *> Function::typesFromMetaArgs(codegen::ModuleContext& ctx, OrderedMap<std::string, std::shared_ptr<Type>> args) {
  std::vector<llvm::Type *> arg_llvm_types;

  std::transform(args.begin(), args.end(), std::back_inserter(arg_llvm_types), [&](auto key) -> auto {
    return args[key]->getLLVMType(ctx);
  });

  return arg_llvm_types;
}

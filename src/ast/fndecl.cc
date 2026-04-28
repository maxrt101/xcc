#include "xcc/ast/fndecl.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/util/log.h"

using namespace xcc;
using namespace xcc::ast;

static auto logger = xcc::util::log::Logger("FNDECL");

FnDecl::FnDecl(
    std::shared_ptr<Identifier> name,
    std::shared_ptr<Type> return_type,
    std::vector<std::shared_ptr<TypedIdentifier>> args,
    bool isExtern,
    bool isVariadic
) : Node(AST_FUNCTION_DECL),
    name(std::move(name)),
    return_type(std::move(return_type)),
    args(std::move(args)),
    isExtern(isExtern),
    isVariadic(isVariadic) {}

std::shared_ptr<FnDecl> FnDecl::create(
    std::shared_ptr<Identifier> name,
    std::shared_ptr<Type> return_type,
    std::vector<std::shared_ptr<TypedIdentifier>> args,
    bool isExtern,
    bool isVariadic
) {
  return std::make_shared<FnDecl>(std::move(name), std::move(return_type), std::move(args), isExtern, isVariadic);
}

llvm::Function * FnDecl::generateFunction(codegen::ModuleContext& ctx, PayloadList payload) {
  std::string fn_name = name->name();

  if (hasAttribute("alias")) {
    auto attr = getAttribute("alias");

    assertThrow(attr.args.size() == 1, CodegenException("Expected 1 argument in 'alias' attribute"));
    assertThrow(attr.args[0]->is(AST_EXPR_IDENTIFIER), CodegenException("Argument to 'alias' attribute must be an identifier"));

    alias_to = attr.args[0]->as<Identifier>()->value;

    logger.debug("Aliasing '{}' as '{}'", fn_name, alias_to);
  }

  OrderedMap<std::string, std::shared_ptr<xcc::meta::Type>> arg_meta_types;

  for (auto& arg : args) {
    if (arg->is(AST_EXPR_TYPED_IDENTIFIER)) {
      arg_meta_types[arg->name->name()] = arg->generateType(ctx, {});
    } else {
      throw CodegenException("Unexpected node discovered in '" + fn_name + "' functions argument ('" + Node::typeToString(arg->type) + "')");
    }
  }

  auto return_meta_type = return_type->generateType(ctx, {});

  auto llvm_fn_type = llvm::FunctionType::get(return_meta_type->getLLVMType(ctx), meta::Function::typesFromMetaArgs(ctx, arg_meta_types), isVariadic);
  // auto llvm_fn = llvm::Function::Create(llvm_fn_type, isExtern ? llvm::Function::ExternalLinkage : llvm::Function::CommonLinkage, fn_name, ctx.llvm.module.get());
  // TODO: LLVM Disallows CommonLinkage, maybe replace with Private?
  auto llvm_fn = llvm::Function::Create(llvm_fn_type, llvm::Function::ExternalLinkage, alias_to.empty() ? fn_name : alias_to, ctx.llvm.module.get());

  size_t arg_idx = 0;
  for (auto& arg : llvm_fn->args()) {
    arg.setName(args[arg_idx++]->name->name());
  }

  auto fn = meta::Function::create(
      fn_name,
      return_meta_type,
      arg_meta_types,
      shared_from_this()
  );

  fn->alias_to = alias_to;

  ctx.globalContext.addFunction(fn_name, fn);

  return llvm_fn;
}

std::shared_ptr<meta::Type> FnDecl::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  std::vector<std::shared_ptr<meta::Type>> args;

  for (auto& arg : this->args) {
    args.push_back(arg->generateType(ctx, payload));
  }

  return meta::Type::createFunction(return_type->generateType(ctx, payload), args, isVariadic);
}

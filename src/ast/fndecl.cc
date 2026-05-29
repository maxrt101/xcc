#include "xcc/ast/fndecl.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/util/log.h"

using namespace xcc;
using namespace xcc::ast;

static auto& logger = xcc::log::Logger::get("FN");
static auto& alias_logger = xcc::log::Logger::get("ALIAS");

FnDecl::FnDecl(
  SourceSpan                                    span,
  LexicalScope                                  scope,
  std::shared_ptr<Node>                         name,
  std::shared_ptr<Node>                         return_type,
  std::vector<std::shared_ptr<TypedIdentifier>> args,
  bool                                          isExtern,
  bool                                          isVariadic
) : Node(AST_FUNCTION_DECL, span, scope),
    name(std::move(name)),
    return_type(std::move(return_type)),
    args(std::move(args)),
    isExtern(isExtern),
    isVariadic(isVariadic) {}

std::shared_ptr<FnDecl> FnDecl::create(
  SourceSpan                                    span,
  LexicalScope                                  scope,
  std::shared_ptr<Node>                         name,
  std::shared_ptr<Node>                         return_type,
  std::vector<std::shared_ptr<TypedIdentifier>> args,
  bool                                          isExtern,
  bool                                          isVariadic
) {
  return std::make_shared<FnDecl>(span, scope, std::move(name), std::move(return_type), std::move(args), isExtern, isVariadic);
}

std::shared_ptr<Node> FnDecl::clone() {
  return withAttrs(create(
    span, scope,
    cast<Identifier>(name->clone()),
    return_type->clone(),
    cloneVector(args),
    isExtern, isVariadic
  ));
}

void FnDecl::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(globalContext, name, visitor, ignoreSubtree);
  callVisitor(globalContext, return_type, visitor, ignoreSubtree);

  for (auto& node : args) {
    callVisitor(globalContext, node, visitor, ignoreSubtree);
  }
}

std::string FnDecl::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(indent, newline) + std::format("{}fn {}(",
    isExtern ? "extern " : "",
    name->toString(parent, this, indent, false)
  );

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

  return res;
}

llvm::Function * FnDecl::generateFunction(codegen::ModuleContext& ctx, PayloadList payload) {
  assertRaiseFromNode(name->is(AST_EXPR_IDENTIFIER), Error(ERROR_FN_NAME_NOT_AN_IDENTIFIER, name->span), this);

  auto id = name->as<Identifier>();

  std::string fn_name = isExtern ? id->value : getMangledName(id->value);
  std::string symbol_name = fn_name;

  if (hasAttribute("alias")) {
    auto attr = getAttribute("alias");

    attr.validateArgs({{AST_EXPR_IDENTIFIER, AST_EXPR_STRING}});

    alias_to = attr.args[0]->is(AST_EXPR_IDENTIFIER)
      ? attr.args[0]->as<Identifier>()->name()
      : attr.args[0]->as<String>()->value;

    symbol_name = alias_to;
  }

  // Prevent regeneration on subsequent passes
  if (auto * existing = ctx.llvm.module->getFunction(symbol_name)) {
    return existing;
  }

  if (!alias_to.empty()) {
    alias_logger.debug("Aliasing '{}' as '{}'", fn_name, alias_to);
  }

  OrderedMap<std::string, std::shared_ptr<meta::Type>> arg_meta_types;

  for (auto& arg : args) {
    if (arg->is(AST_EXPR_TYPED_IDENTIFIER)) {
      arg_meta_types[arg->name->name()] = arg->generateType(ctx, payload);
    } else {
      Error(ERROR_INTERNAL_UNEXPECTED_NODE, arg->span, "Unexpected {} in function '{}' argument declaration", typeToHumanReadableString(arg->type), fn_name).raiseFromNode(this);
    }
  }

  auto return_meta_type = return_type->generateType(ctx, payload);

  auto llvm_fn_type = llvm::FunctionType::get(return_meta_type->getLLVMType(ctx), meta::Function::typesFromMetaArgs(ctx, arg_meta_types), isVariadic);
  // auto llvm_fn = llvm::Function::Create(llvm_fn_type, isExtern ? llvm::Function::ExternalLinkage : llvm::Function::CommonLinkage, fn_name, ctx.llvm.module.get());
  // TODO: LLVM Disallows CommonLinkage, maybe replace with Private?
  auto llvm_fn = llvm::Function::Create(llvm_fn_type, llvm::Function::ExternalLinkage, symbol_name, ctx.llvm.module.get());

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

  ctx.globalContext.addFunction(fn_name, fn, meta::Type::createFunction(return_meta_type, arg_meta_types.values(), isVariadic));

  return llvm_fn;
}

std::shared_ptr<meta::Type> FnDecl::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  std::vector<std::shared_ptr<meta::Type>> args;

  for (auto& arg : this->args) {
    args.push_back(arg->generateType(ctx, payload));
  }

  return meta::Type::createFunction(return_type->generateType(ctx, payload), args, isVariadic);
}

std::shared_ptr<TypedIdentifier> FnDecl::getArgument(const std::string& name) {
  for (auto& arg : args) {
    if (arg->name->value == name) {
      return arg;
    }
  }

  return nullptr;
}

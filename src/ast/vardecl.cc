#include "xcc/ast/vardecl.h"
#include "xcc/ast/number.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;

VarDecl::VarDecl(
  SourceSpan                  span,
  std::shared_ptr<Identifier> name,
  std::shared_ptr<Node>       type,
  std::shared_ptr<Node>       value,
  bool                        global
) : Node(AST_VAR_DECL, span),
    name(std::move(name)),
    type(std::move(type)),
    value(std::move(value)),
    global(global) {}

std::shared_ptr<VarDecl> VarDecl::create(
  SourceSpan                  span,
  std::shared_ptr<Identifier> name,
  std::shared_ptr<Node>       type,
  std::shared_ptr<Node>       value,
  bool                        global
) {
  return std::make_shared<VarDecl>(span, std::move(name), std::move(type), std::move(value), global);
}

std::shared_ptr<Node> VarDecl::clone() {
  return withAttrs(create(span,
    cast<Identifier>(name->clone()),
    type ? type->clone() : nullptr,
    value ? value->clone() : nullptr,
    global
  ));
}

void VarDecl::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(name, visitor, ignoreSubtree);
  callVisitor(type, visitor, ignoreSubtree);
  callVisitor(value, visitor, ignoreSubtree);
}

std::string VarDecl::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(indent, newline) + "var " + name->toString(parent, this, indent, false);

  if (type) {
    res += ": " + type->toString(parent, this, indent, false);
  }

  if (value) {
    res += " = " + value->toString(parent, this, indent, false);
  }

  return res;
}

llvm::Value * VarDecl::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  // If both type and value are missing - fail, if one is present - the other can be (usually) inferred
  assertRaiseFromNode(type || value, Error(ERROR_VARDECL_NO_VAL_AND_TYPE, span), this);

  if (type) {
    assertRaiseFromNode(isOrIsLastInBlock(type, AST_EXPR_TYPE), Error(ERROR_NOT_A_TYPE, type->span, "got a {}", typeToHumanReadableString(getOrGetLastInBlock(type)->type)), this);
  }

  auto meta_type = type ? type->generateType(ctx, {}) : meta::Type::inferFromNode(ctx, value);

  return global ? generateGlobal(ctx, meta_type) : generateLocal(ctx, meta_type);
}

std::shared_ptr<xcc::meta::Type> VarDecl::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  return type->generateType(ctx, {});
}

llvm::Value * VarDecl::generateLocal(codegen::ModuleContext& ctx, std::shared_ptr<meta::Type> meta_type) {
  auto fn = ctx.ir_builder->GetInsertBlock()->getParent();

  llvm::Value * init = value
      ? value->generateValue(ctx, {Number::Payload::create(meta_type->getNumberBitWidth())})
      : meta_type->getDefault(ctx);

  init = codegen::castIfNotSame(ctx, init, meta_type->getLLVMType(ctx), value ? value->span : span);

  auto tv = meta::TypedValue::create(ctx, fn, span, meta_type, name->name());

  auto di_local = ctx.globalContext.di_builder->createAutoVariable(
      ctx.currentDIScope(),
      name->name(),
      ctx.globalContext.getCurrentDIFile(),
      span.start().line,
      meta_type->getDIType(ctx),
      true
    );

  ctx.globalContext.di_builder->insertDeclare(
    tv->value,
    di_local,
    ctx.globalContext.di_builder->createExpression(),
    span.start().getDILocation(ctx),
    ctx.ir_builder->GetInsertBlock()
  );

  ctx.ir_builder->CreateStore(init, tv->value);

  ctx.addLocal(name->name(), tv);

  return init;
}

llvm::Value * VarDecl::generateGlobal(codegen::ModuleContext& ctx, std::shared_ptr<meta::Type> meta_type) {
  auto llvm_type = meta_type->getLLVMType(ctx);
  auto constant  = (llvm::Constant *)(value
      ? value->generateConstant(ctx, {Number::Payload::create(meta_type->getNumberBitWidth())})
      : meta_type->getDefault(ctx));

  ctx.globalContext.globals[name->name()] = meta_type;

  [[maybe_unused]] auto global = new llvm::GlobalVariable(
      *ctx.globalContext.globalModule->llvm.module,
      llvm_type,
      false,
      llvm::GlobalValue::ExternalLinkage,
      constant,
      name->name()
  );

  auto extern_global = llvm::cast<llvm::GlobalVariable>(
    ctx.llvm.module->getOrInsertGlobal(name->name(), llvm_type));

  extern_global->print(llvm::outs());

  auto di_global = ctx.globalContext.di_builder->createTempGlobalVariableFwdDecl(
    ctx.currentDIScope(),
    name->name(),
    name->name(),
    ctx.globalContext.getCurrentDIFile(),
    span.start().line,
    meta_type->getDIType(ctx),
    true
  );

  // TODO: How to add global variable?
  // ctx.globalContext.di_builder->insertDeclare(
  //   global,
  //   di_global,
  //   ctx.globalContext.di_builder->createExpression(),
  //   span.start().getDILocation(ctx),
  //   ctx.ir_builder->GetInsertBlock()
  // );

  return extern_global;
}

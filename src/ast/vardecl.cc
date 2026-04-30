#include "xcc/ast/vardecl.h"
#include "xcc/ast/number.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;

VarDecl::VarDecl(
  std::shared_ptr<Identifier> name,
  std::shared_ptr<Node> type,
  std::shared_ptr<Node> value,
  bool global
) : Node(AST_VAR_DECL),
    name(std::move(name)),
    type(std::move(type)),
    value(std::move(value)),
    global(global) {}

std::shared_ptr<VarDecl> VarDecl::create(
    std::shared_ptr<Identifier> name,
    std::shared_ptr<Node> type,
    std::shared_ptr<Node> value,
    bool global
) {
  return std::make_shared<VarDecl>(std::move(name), std::move(type), std::move(value), global);
}

std::shared_ptr<Node> VarDecl::clone() {
  return withAttrs(create(cast<Identifier>(
    name->clone()),
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
  std::string res = "var " + name->toString(parent, this, indent, false);

  if (type) {
    res += ": " + type->toString(parent, this, indent, false);
  }

  if (value) {
    res += " = " + value->toString(parent, this, indent, false);
  }

  return res;
}

llvm::Value * VarDecl::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  // If both type and value are missing - fail, if one is present - the other can be (usually) inferred
  assertThrow(type || value, CodegenException("Value and type is missing from variable declaration"));

  auto meta_type = type ? type->generateType(ctx, {}) : meta::Type::inferFromNode(ctx, value);

  if (global) {
    // FIXME: Check if can convert to constant

    auto constant = (llvm::Constant *)(value
        ? value->generateValueWithoutLoad(ctx, {Number::Payload::create(meta_type->getNumberBitWidth())})
        : meta_type->getDefault(ctx));

    ctx.globalContext.globals[name->name()] = meta_type;

    [[maybe_unused]] auto global = new llvm::GlobalVariable(
        *ctx.globalContext.globalModule->llvm.module,
        constant->getType(),
        false,
        llvm::GlobalValue::ExternalLinkage,
        constant,
        name->name()
    );

    auto extern_global = llvm::cast<llvm::GlobalVariable>(
      ctx.llvm.module->getOrInsertGlobal(name->name(), llvm::Type::getInt32Ty(*ctx.llvm.ctx)));

    return extern_global;
  }

  auto fn = ctx.ir_builder->GetInsertBlock()->getParent();

  llvm::Value * init = value
      ? value->generateValue(ctx, {Number::Payload::create(meta_type->getNumberBitWidth())})
      : meta_type->getDefault(ctx);

  init = codegen::castIfNotSame(ctx, init, meta_type->getLLVMType(ctx));

  auto tv = meta::TypedValue::create(ctx, fn, meta_type, name->name());

  ctx.ir_builder->CreateStore(init, tv->value);

  ctx.locals[name->name()] = tv;

  return init;
}

std::shared_ptr<xcc::meta::Type> VarDecl::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  return type->generateType(ctx, {});
}

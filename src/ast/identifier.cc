#include "xcc/ast/identifier.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc;
using namespace xcc::ast;

Identifier::Identifier(SourceSpan span, std::string value, std::vector<std::string> scope)
  : Node(AST_EXPR_IDENTIFIER, span), value(std::move(value)), scope(std::move(scope)) {}

std::shared_ptr<Identifier> Identifier::create(SourceSpan span, const std::string& value, std::vector<std::string> scope) {
  return std::make_shared<Identifier>(span, value, scope);
}

std::shared_ptr<Node> Identifier::clone() {
  return withAttrs(create(span, value, scope));
}

void Identifier::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {}

std::string Identifier::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(0, false);

  for (auto& part : scope) {
    res += part + "::";
  }

  return res + value;
}

std::string Identifier::name() const {
  std::string prefix;

  for (auto& part : scope) {
    prefix += part + "_";
  }

  return prefix + value;
}

llvm::Value * Identifier::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  auto id = ctx.globalContext.aliased(name());

  if (ctx.hasLocal(id)) {
    auto local = ctx.getLocalValue(id);
    if (local->getAllocatedType()->isArrayTy()) {
      return local;
    }

    return ctx.ir_builder->CreateLoad(local->getAllocatedType(), local, id.c_str());
  }

  if (ctx.globalContext.hasGlobal(id)) {
    auto global    = ctx.globalContext.getGlobal(ctx, id);
    auto meta_type = ctx.globalContext.getGlobalType(id);

    if (meta_type->isArray()) {
      return global;
    }

    return ctx.ir_builder->CreateLoad(
      meta_type->getLLVMType(ctx),
      global,
      id.c_str()
    );
  }

  if (auto * fn = ctx.getFunction(id)) {
    return fn;
  }

  Error(ERROR_UNDECLARED_VALUE, span, "'{}'", id).raiseFromNode(this);
}

llvm::Value * Identifier::generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  auto id = ctx.globalContext.aliased(name());

  if (ctx.hasLocal(id)) {
    return ctx.getLocalValue(id);
  }

  if (ctx.globalContext.hasGlobal(id)) {
    return ctx.globalContext.getGlobal(ctx, id);
  }

  if (auto * fn = ctx.getFunction(id)) {
    return fn;
  }

  Error(ERROR_UNDECLARED_VALUE, span, "'{}'", id).raiseFromNode(this);
}

std::shared_ptr<meta::Type> Identifier::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  return generateTypeForValueWithoutLoad(ctx, payload);
}

std::shared_ptr<xcc::meta::Type> Identifier::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  auto id = ctx.globalContext.aliased(name());

  if (ctx.hasPhantom(id)) {
    return ctx.getPhantomType(id);
  }

  if (ctx.hasLocal(id)) {
    return ctx.getLocalType(id);
  }

  if (ctx.globalContext.hasGlobal(id)) {
    return ctx.globalContext.getGlobalType(id);
  }

  if (auto meta_fn = ctx.globalContext.getMetaFunction(id)) {
    return meta_fn->decl->generateType(ctx, payload);
  }

  Error(ERROR_UNDECLARED_VALUE, span, "'{}'", id).raiseFromNode(this);
}

#include "xcc/ast/string.h"
#include "xcc/codegen.h"
#include "xcc/util/string.h"

using namespace xcc::ast;

String::String(SourceSpan span, std::string value) : Node(AST_EXPR_STRING, span), value(std::move(value)) {}

std::shared_ptr<String> String::create(SourceSpan span, std::string value) {
  return std::make_shared<String>(span, std::move(value));
}

std::shared_ptr<Node> String::clone() {
  return withAttrs(create(span, value));
}

void String::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {}

std::string String::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(0, false) + std::format("\"{}\"", util::strescseq(value, false));
}

llvm::Value * String::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  auto str = getOrCreateGlobalString(ctx);

  llvm::Constant * zero = ctx.ir_builder->getInt32(0);

  return llvm::ConstantExpr::getInBoundsGetElementPtr(
      str.global->getValueType(),
      str.global,
      llvm::ArrayRef<llvm::Constant*> {zero, zero}
  );
}

llvm::Value * String::generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  return llvm::ConstantDataArray::getString(*ctx.globalContext.globalModule->llvm.ctx, value, true);
}

llvm::Constant * String::generateConstant(codegen::ModuleContext& ctx, PayloadList payload) {
  auto str = getOrCreateGlobalString(ctx);

  llvm::Type *     array_type = str.global->getValueType();
  llvm::Constant * zero       = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx.llvm.ctx), 0);

  return llvm::ConstantExpr::getInBoundsGetElementPtr(array_type, str.global, llvm::ArrayRef<llvm::Constant*> {zero, zero});
}

std::shared_ptr<xcc::meta::Type> String::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  return meta::Type::createPointer(meta::Type::createI8());
}

String::GlobalString String::getOrCreateGlobalString(codegen::ModuleContext& ctx) {
  auto hash = std::hash<std::string>{}(value);
  auto name = ".str." + std::to_string(hash);

  if (auto * existing = ctx.llvm.module->getNamedGlobal(name)) {
    return {name, existing};
  }

  llvm::Constant * str_const = llvm::ConstantDataArray::getString(*ctx.llvm.ctx, value, true);

  return {
    name,
    new llvm::GlobalVariable(
      *ctx.llvm.module,
      str_const->getType(),
      true,
      llvm::GlobalValue::ExternalLinkage,
      str_const,
      name
    )
  };
}

#include "xcc/ast/asm.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

Asm::Asm(
  SourceSpan                         span,
  std::shared_ptr<Node>              code,
  std::shared_ptr<Node>              constraints,
  std::vector<std::shared_ptr<Node>> args
) : Node(AST_ASM, span), code(std::move(code)), constraints(std::move(constraints)), args(std::move(args)) {}

std::shared_ptr<Asm> Asm::create(
  SourceSpan                         span,
  std::shared_ptr<Node>              code,
  std::shared_ptr<Node>              constraints,
  std::vector<std::shared_ptr<Node>> args
) {
  return std::make_shared<Asm>(span, std::move(code), std::move(constraints), std::move(args));
}

std::shared_ptr<Node> Asm::clone() {
  return withAttrs(create(span, code->clone(), constraints->clone(), cloneVector(args)));
}

void Asm::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(code, visitor, ignoreSubtree);
  callVisitor(constraints, visitor, ignoreSubtree);
}

std::string Asm::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string arg_str;

  for (size_t i = 0; i < args.size(); ++i) {
    arg_str += args[i]->toString(parent, this, indent, false);
    if (i + 1 < args.size()) {
      arg_str += ", ";
    }
  }

  return attributesToString(0, false) + std::format("asm!({}, {}{})",
    code->toString(parent, this, indent, false),
    constraints->toString(parent, this, indent, false),
    arg_str.empty() ? "" : (", " + arg_str)
  );
}

llvm::Value * Asm::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  assertRaiseFromNode(isOrIsLastInBlock(code, AST_EXPR_STRING),
    Error(ERROR_ASM_EXPECTED_STRING, code->span, "asm! code must be a string, got {}", typeToHumanReadableString(code->type)), this);

  assertRaiseFromNode(isOrIsLastInBlock(constraints, AST_EXPR_STRING),
    Error(ERROR_ASM_EXPECTED_STRING, constraints->span, "asm! constraints must be a string, got {}", typeToHumanReadableString(constraints->type)), this);

  auto code_val        = code->as<String>()->value;
  auto constraints_val = constraints->as<String>()->value;

  std::vector<llvm::Value *> arg_values;
  std::vector<llvm::Type *>  arg_types;

  for (auto& arg : args) {
    auto val = arg->generateValue(ctx, {});
    arg_values.push_back(val);
    arg_types.push_back(val->getType());
  }

  auto return_type = llvm::Type::getVoidTy(*ctx.llvm.ctx);

  // TODO: Parse all constraints
  if (!constraints_val.empty() && constraints_val[0] == '=') {
    // TODO: i32 may not be valid in all of the cases
    return_type = llvm::Type::getInt32Ty(*ctx.llvm.ctx);
  }

  auto fn_type = llvm::FunctionType::get(return_type, arg_types, false);
  auto ia      = llvm::InlineAsm::get(fn_type, code_val, constraints_val, true);
  auto call    = ctx.ir_builder->CreateCall(fn_type, ia, arg_values);

  return return_type->isVoidTy() ? nullptr : call;
}

std::shared_ptr<xcc::meta::Type> Asm::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  return meta::Type::createVoid();
}

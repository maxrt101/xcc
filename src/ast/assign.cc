#include "xcc/ast/assign.h"
#include "xcc/ast/number.h"
#include "xcc/ast/binary.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;
using namespace xcc;

static std::unordered_map<TokenType, TokenType> s_equal_to_op = {
  {TOKEN_ADD_EQUALS,          TOKEN_PLUS},
  {TOKEN_MIN_EQUALS,          TOKEN_MINUS},
  {TOKEN_MUL_EQUALS,          TOKEN_STAR},
  {TOKEN_DIV_EQUALS,          TOKEN_SLASH},
  {TOKEN_AND_EQUALS,          TOKEN_AMP},
  {TOKEN_OR_EQUALS,           TOKEN_VERTICAL_LINE},
  {TOKEN_LOGICAL_AND_EQUALS,  TOKEN_AND},
  {TOKEN_LOGICAL_OR_EQUALS,   TOKEN_OR},
};

Assign::Assign(SourceSpan span, LexicalScope scope, Token kind, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
  : Node(AST_EXPR_ASSIGN, span, scope), kind(std::move(kind)), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

std::shared_ptr<Assign> Assign::create(SourceSpan span, LexicalScope scope, Token kind, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs) {
  return std::make_shared<Assign>(span, scope, std::move(kind), std::move(lhs), std::move(rhs));
}

std::shared_ptr<Node> Assign::clone() {
  return withAttrs(create(span, scope, kind, lhs->clone(), rhs->clone()));
}

void Assign::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(globalContext, lhs, visitor, ignoreSubtree);
  callVisitor(globalContext, rhs, visitor, ignoreSubtree);
}

std::string Assign::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(0, false) + std::format("{} = {}",
    lhs->toString(parent, this, indent, false),
    rhs->toString(parent, this, indent, false)
  );
}

llvm::Value * Assign::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  llvm::Value * value = nullptr;

  if (kind.type == TOKEN_EQUALS) {
    value = rhs->generateValue(ctx, payload);
  } else if (s_equal_to_op.find(kind.type) != s_equal_to_op.end()) {
    value = Binary::create(span, scope, kind.clone(s_equal_to_op[kind.type]), lhs, rhs)->generateValue(ctx, payload);
  } else {
    Error(ERROR_INVALID_ASSIGNMENT_OP, kind.span, "{}", Token::typeToString(kind.type)).raiseFromNode(this);
  }

  auto lhs_meta_type = lhs->generateTypeForValueWithoutLoad(ctx, payload);
  auto lhs_llvm_type = lhs_meta_type->getLLVMType(ctx);
  auto lhs_ptr       = lhs->generateValueWithoutLoad(ctx, payload);

  value = codegen::castIfNotSame(
    ctx,
    throwIfNull(value, std::runtime_error("assignment value generated NULL")),
    lhs_llvm_type,
    span
  );

  // Call drop on LHS, as it's about to be overwritten
  if (lhs_meta_type->isStruct() && lhs_meta_type->isDrop()) {
    llvm::Function * drop_fn = ctx.getFunction(lhs_meta_type->getDropMethodName());

    // If value can be traced to a named local - update it's state to MOVED
    ctx.updateLocalLiveness(rhs, meta::Liveness::MOVED);

    if (drop_fn) {
      ctx.ir_builder->CreateCall(drop_fn, {lhs_ptr});
    } else {
      Warning(WARNING_STRUCT_DROP_NO_FN, {}, "'{}'", lhs_meta_type->getName()).emit();
    }
  }

  ctx.ir_builder->CreateStore(
    value,
    lhs->generateValueWithoutLoad(ctx, payload)
  );

  // If we got this far, RHS should be the same type as LHS, so check if it needs to be dropped
  if (lhs_meta_type->isStruct() && lhs_meta_type->isDrop()) {
    if (isOrIsLastInBlock(rhs, AST_EXPR_CALL) || isOrIsLastInBlock(rhs, AST_INIT)) {
      // Temporary value - can be just forgotten
      ctx.currentScope().raii.forgetLastTemporary();
    } else if (isOrIsLastInBlock(rhs, AST_EXPR_IDENTIFIER)) {
      // Not a temporary - clear the value manually
      auto rhs_ptr  = rhs->generateValueWithoutLoad(ctx, payload);
      auto zero_val = llvm::ConstantAggregateZero::get(lhs_llvm_type);
      ctx.ir_builder->CreateStore(zero_val, rhs_ptr);
    }
  }

  // Consider LHS valid now
  ctx.updateLocalLiveness(lhs, meta::Liveness::INITIALIZED);

  return lhs->generateValue(ctx, payload);
}

std::shared_ptr<xcc::meta::Type> Assign::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  if (lhs->is(AST_EXPR_IDENTIFIER)) {
    auto name = Node::cast<Identifier>(lhs);

    if (ctx.hasLocal(name->name())) {
      return ctx.getLocalType(name->name());
    }

    Error(ERROR_UNKNOWN_VARIABLE, name->span, "'{}'", name->name()).raiseFromNode(this);
  }

  return lhs->generateTypeForValueWithoutLoad(ctx, payload);
}

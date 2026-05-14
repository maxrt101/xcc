#include "xcc/ast/binary.h"
#include "xcc/meta/binops.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::binop::Conditions;
using namespace xcc::ast;
using namespace xcc;

static const binop::List s_binops = {
  XCC_BINOP(TOKEN_PLUS,           INTEGER,            CreateAdd,         "addtmp",      (bool, bool)      ),
  XCC_BINOP(TOKEN_PLUS,           FLOAT,              CreateFAdd,        "addftmp",     ()                ),
  XCC_BINOP(TOKEN_MINUS,          INTEGER,            CreateSub,         "subtmp",      (bool, bool)      ),
  XCC_BINOP(TOKEN_MINUS,          FLOAT,              CreateFSub,        "subftmp",     (llvm::MDNode*)   ),
  XCC_BINOP(TOKEN_STAR,           INTEGER,            CreateMul,         "multmp",      (bool, bool)      ),
  XCC_BINOP(TOKEN_STAR,           FLOAT,              CreateFMul,        "mulftmp",     (llvm::MDNode*)   ),
  XCC_BINOP(TOKEN_SLASH,          INTEGER | SIGNED,   CreateSDiv,        "divstmp",     (bool)            ),
  XCC_BINOP(TOKEN_SLASH,          INTEGER | UNSIGNED, CreateUDiv,        "divutmp",     (bool)            ),
  XCC_BINOP(TOKEN_SLASH,          FLOAT,              CreateFDiv,        "divftmp",     (llvm::MDNode*)   ),
  XCC_BINOP(TOKEN_EQUALS_EQUALS,  INTEGER,            CreateICmpEQ,      "eqcmptmp",    ()                ),
  XCC_BINOP(TOKEN_EQUALS_EQUALS,  FLOAT,              CreateFCmpUEQ,     "eqcmpftmp",   (llvm::MDNode *)  ),
  XCC_BINOP(TOKEN_NOT_EQUALS,     INTEGER,            CreateICmpNE,      "neqcmptmp",   ()                ),
  XCC_BINOP(TOKEN_NOT_EQUALS,     FLOAT,              CreateFCmpUNE,     "neqcmpftmp",  (llvm::MDNode *)  ),
  XCC_BINOP(TOKEN_GREATER_EQUALS, INTEGER,            CreateICmpUGE,     "gecmptmp",    ()                ),
  XCC_BINOP(TOKEN_GREATER_EQUALS, FLOAT,              CreateFCmpUGE,     "gecmpftmp",   (llvm::MDNode *)  ),
  XCC_BINOP(TOKEN_GREATER,        INTEGER,            CreateICmpUGT,     "gtcmptmp",    ()                ),
  XCC_BINOP(TOKEN_GREATER,        FLOAT,              CreateFCmpUGT,     "gtcmpftmp",   (llvm::MDNode *)  ),
  XCC_BINOP(TOKEN_LESS_EQUALS,    INTEGER,            CreateICmpULE,     "lecmptmp",    ()                ),
  XCC_BINOP(TOKEN_LESS_EQUALS,    FLOAT,              CreateFCmpULE,     "lecmpftmp",   (llvm::MDNode *)  ),
  XCC_BINOP(TOKEN_LESS,           INTEGER,            CreateICmpULT,     "ltcmptmp",    ()                ),
  XCC_BINOP(TOKEN_LESS,           FLOAT,              CreateFCmpULT,     "ltcmpftmp",   (llvm::MDNode *)  ),
  XCC_BINOP(TOKEN_AND,            NONE,               CreateLogicalAnd,  "landtmp",     ()                ),
  XCC_BINOP(TOKEN_OR,             NONE,               CreateLogicalOr,   "lortmp",      ()                ),
  XCC_BINOP(TOKEN_AMP,            NONE,               CreateAnd,         "andtmp",      ()                ),
  XCC_BINOP(TOKEN_VERTICAL_LINE,  NONE,               CreateOr,          "ortmp",       ()                ),
};

Binary::Binary(SourceSpan span, Token operation, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
  : Node(AST_EXPR_BINARY, span), operation(std::move(operation)), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

std::shared_ptr<Binary> Binary::create(SourceSpan span, Token operation, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs) {
  return std::make_shared<Binary>(span, std::move(operation), std::move(lhs), std::move(rhs));
}

std::shared_ptr<Node> Binary::clone() {
  return withAttrs(create(span, operation, lhs->clone(), rhs->clone()));
}

void Binary::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(lhs, visitor, ignoreSubtree);
  callVisitor(rhs, visitor, ignoreSubtree);
}

std::string Binary::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(0, false) + std::format("{} {} {}",
    lhs->toString(parent, this, indent, false),
    operation.toString(),
    rhs->toString(parent, this, indent, false)
  );
}

llvm::Value * Binary::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  auto common_type = meta::Type::alignTypes(
    raiseIfNull(lhs->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, lhs->span, "LHS Type is NULL")),
    raiseIfNull(rhs->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, rhs->span, "RHS Type is NULL"))
  );

  // Pointer comparisons are actually converted to integer
  if (common_type->isPointer()) {
    common_type = meta::Type::createU64();
  }

  auto lhs_val = castIfNotSame(
    ctx,
    raiseIfNull(
      lhs->generateValue(ctx, payload),
      Error(ERROR_INTERNAL_UNEXPECTED_NULL, lhs->span, "LHS Value is NULL")
    ),
    common_type->getLLVMType(ctx),
    lhs->span
  );

  auto rhs_val = castIfNotSame(
    ctx,
    raiseIfNull(
      rhs->generateValue(ctx, payload),
      Error(ERROR_INTERNAL_UNEXPECTED_NULL, rhs->span, "RHS Value is NULL")
    ),
    common_type->getLLVMType(ctx),
    rhs->span
  );

  if (auto binop = findBinaryOperation(s_binops, binop::Meta::fromType(operation.type, common_type))) {
    return binop->handler(ctx, lhs_val, rhs_val, binop->twine);
  }

  Error(ERROR_UNKNOWN_BIN_OP_OR_TYPE, operation.span, "op={} type={}", operation.toString(), common_type->toString())
    .raiseFromNode(this);
}

llvm::Constant * Binary::generateConstant(codegen::ModuleContext& ctx, PayloadList payload) {
  auto lhs_type = raiseIfNull(lhs->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, rhs->span, "LHS Type is NULL"));
  auto rhs_type = raiseIfNull(rhs->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, rhs->span, "RHS Type is NULL"));

  assertRaiseFromNode(lhs_type->isInteger() && rhs_type->isInteger(),
    Error(ERROR_UNIMPLEMENTED, span, "Only integers are supported for constant binary expressions"), this);

  auto meta_type = meta::Type::alignTypes(lhs_type, rhs_type);
  auto t         = meta_type->getLLVMType(ctx);

  auto l_val = lhs->generateConstant(ctx, payload);
  auto l     = llvm::dyn_cast<llvm::ConstantInt>(l_val);

  auto r_val = rhs->generateConstant(ctx, payload);
  auto r     = llvm::dyn_cast<llvm::ConstantInt>(r_val);

  switch (operation.type) {
    case TOKEN_PLUS:           return llvm::ConstantInt::get(t, l->getValue() + r->getValue());
    case TOKEN_MINUS:          return llvm::ConstantInt::get(t, l->getValue() - r->getValue());
    case TOKEN_STAR:           return llvm::ConstantInt::get(t, l->getValue() * r->getValue());
    case TOKEN_SLASH:          return llvm::ConstantInt::get(t, l->getValue().sdiv(r->getValue()));
    case TOKEN_EQUALS_EQUALS:  return llvm::ConstantInt::get(t, l->getValue() == r->getValue());
    case TOKEN_NOT_EQUALS:     return llvm::ConstantInt::get(t, l->getValue() != r->getValue());
    case TOKEN_GREATER_EQUALS: return llvm::ConstantInt::get(t, l->getValue().sgt(r->getValue()));
    case TOKEN_GREATER:        return llvm::ConstantInt::get(t, l->getValue().sge(r->getValue()));
    case TOKEN_LESS_EQUALS:    return llvm::ConstantInt::get(t, l->getValue().sle(r->getValue()));
    case TOKEN_LESS:           return llvm::ConstantInt::get(t, l->getValue().slt(r->getValue()));
    case TOKEN_AND:            return llvm::ConstantInt::get(t, l->getValue().getBoolValue() && r->getValue().getBoolValue());
    case TOKEN_OR:             return llvm::ConstantInt::get(t, l->getValue().getBoolValue() || r->getValue().getBoolValue());
    case TOKEN_AMP:            return llvm::ConstantInt::get(t, l->getValue() & r->getValue());
    case TOKEN_VERTICAL_LINE:  return llvm::ConstantInt::get(t, l->getValue() | r->getValue());
    default:
    break;
  }

  Error(ERROR_UNKNOWN_BIN_OP_OR_TYPE, operation.span, "op={} type={}", operation.toString(), meta_type->toString())
    .raiseFromNode(this);
}

std::shared_ptr<meta::Type> Binary::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  auto lhs_type = raiseIfNull(lhs->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, lhs->span, "LHS type is NULL"));
  auto rhs_type = raiseIfNull(rhs->generateType(ctx, payload), Error(ERROR_INTERNAL_UNEXPECTED_NULL, rhs->span, "RHS type is NULL"));

  return meta::Type::alignTypes(lhs_type, rhs_type);
}

#include "xcc/ast/binary.h"
#include "xcc/meta/binops.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::BinaryOperationConditions;
using namespace xcc::ast;
using namespace xcc;

static const BinaryOperations s_binops = {
  XCC_BINOP(TOKEN_PLUS,           INTEGER,            ctx.ir_builder->CreateAdd(lhs, rhs, "addtmp")),
  XCC_BINOP(TOKEN_PLUS,           FLOAT,              ctx.ir_builder->CreateFAdd(lhs, rhs, "addftmp")),
  XCC_BINOP(TOKEN_MINUS,          INTEGER,            ctx.ir_builder->CreateSub(lhs, rhs, "subtmp")),
  XCC_BINOP(TOKEN_MINUS,          FLOAT,              ctx.ir_builder->CreateFSub(lhs, rhs, "subftmp")),
  XCC_BINOP(TOKEN_STAR,           INTEGER,            ctx.ir_builder->CreateMul(lhs, rhs, "multmp")),
  XCC_BINOP(TOKEN_STAR,           FLOAT,              ctx.ir_builder->CreateFMul(lhs, rhs, "mulftmp")),
  XCC_BINOP(TOKEN_SLASH,          INTEGER | SIGNED,   ctx.ir_builder->CreateSDiv(lhs, rhs, "divstmp")),
  XCC_BINOP(TOKEN_SLASH,          INTEGER | UNSIGNED, ctx.ir_builder->CreateUDiv(lhs, rhs, "divutmp")),
  XCC_BINOP(TOKEN_SLASH,          FLOAT,              ctx.ir_builder->CreateFDiv(lhs, rhs, "divftmp")),
  XCC_BINOP(TOKEN_EQUALS_EQUALS,  INTEGER,            ctx.ir_builder->CreateICmpEQ(lhs, rhs, "eqcmptmp")),
  XCC_BINOP(TOKEN_EQUALS_EQUALS,  FLOAT,              ctx.ir_builder->CreateFCmpUEQ(lhs, rhs, "eqcmpftmp")),
  XCC_BINOP(TOKEN_NOT_EQUALS,     INTEGER,            ctx.ir_builder->CreateICmpNE(lhs, rhs, "neqcmptmp")),
  XCC_BINOP(TOKEN_NOT_EQUALS,     FLOAT,              ctx.ir_builder->CreateFCmpUNE(lhs, rhs, "neqcmpftmp")),
  XCC_BINOP(TOKEN_GREATER_EQUALS, INTEGER,            ctx.ir_builder->CreateICmpUGE(lhs, rhs, "gecmptmp")),
  XCC_BINOP(TOKEN_GREATER_EQUALS, FLOAT,              ctx.ir_builder->CreateFCmpUGE(lhs, rhs, "gecmpftmp")),
  XCC_BINOP(TOKEN_GREATER,        INTEGER,            ctx.ir_builder->CreateICmpUGT(lhs, rhs, "gtcmptmp")),
  XCC_BINOP(TOKEN_GREATER,        FLOAT,              ctx.ir_builder->CreateFCmpUGT(lhs, rhs, "gtcmpftmp")),
  XCC_BINOP(TOKEN_LESS_EQUALS,    INTEGER,            ctx.ir_builder->CreateICmpULE(lhs, rhs, "lecmptmp")),
  XCC_BINOP(TOKEN_LESS_EQUALS,    FLOAT,              ctx.ir_builder->CreateFCmpULE(lhs, rhs, "lecmpftmp")),
  XCC_BINOP(TOKEN_LESS,           INTEGER,            ctx.ir_builder->CreateICmpULT(lhs, rhs, "ltcmptmp")),
  XCC_BINOP(TOKEN_LESS,           FLOAT,              ctx.ir_builder->CreateFCmpULT(lhs, rhs, "ltcmpftmp")),
  XCC_BINOP(TOKEN_AND,            NONE,               ctx.ir_builder->CreateLogicalAnd(lhs, rhs, "landtmp")),
  XCC_BINOP(TOKEN_OR,             NONE,               ctx.ir_builder->CreateLogicalOr(lhs, rhs, "lortmp")),
  XCC_BINOP(TOKEN_AMP,            NONE,               ctx.ir_builder->CreateAnd(lhs, rhs, "andtmp")),
  XCC_BINOP(TOKEN_VERTICAL_LINE,  NONE,               ctx.ir_builder->CreateOr(lhs, rhs, "ortmp")),
};

Binary::Binary(Token operation, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
  : Node(AST_EXPR_BINARY), operation(std::move(operation)), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

std::shared_ptr<Binary> Binary::create(Token operation, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs) {
  return std::make_shared<Binary>(std::move(operation), std::move(lhs), std::move(rhs));
}

llvm::Value * Binary::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  auto common_type = meta::Type::alignTypes(
    throwIfNull(lhs->generateType(ctx, {}), CodegenException("LHS Type is NULL")),
    throwIfNull(rhs->generateType(ctx, {}), CodegenException("RHS Type is NULL"))
  );

  // Pointer comparisons are actually converted to integer
  if (common_type->isPointer()) {
    common_type = meta::Type::createU64();
  }

  auto lhs_val = castIfNotSame(
    ctx,
    throwIfNull(
      lhs->generateValue(ctx, {}),
      CodegenException("LHS Value is NULL")
    ),
    common_type->getLLVMType(ctx)
  );

  auto rhs_val = castIfNotSame(
    ctx,
    throwIfNull(
      rhs->generateValue(ctx, {}),
      CodegenException("RHS Value is NULL")
    ),
    common_type->getLLVMType(ctx)
  );

  if (auto binop = findBinaryOperation(s_binops, BinaryOperationMeta::fromType(operation.type, common_type))) {
    return binop->handler(ctx, lhs_val, rhs_val);
  }

  throw CodegenException(operation.line, "Unsupported binary expression operator or type (op=" + operation.toString() + " type=" + common_type->toString() + ")");
}

std::shared_ptr<meta::Type> Binary::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  auto lhs_type = throwIfNull(lhs->generateType(ctx, {}), CodegenException("LHS type is NULL"));
  auto rhs_type = throwIfNull(rhs->generateType(ctx, {}), CodegenException("RHS type is NULL"));

  return meta::Type::alignTypes(lhs_type, rhs_type);
}

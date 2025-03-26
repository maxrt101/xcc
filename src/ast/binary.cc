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

  if (auto binop = findBinaryOperation(s_binops, binop::Meta::fromType(operation.type, common_type))) {
    return binop->handler(ctx, lhs_val, rhs_val, binop->twine);
  }

  throw CodegenException(operation.line, "Unsupported binary expression operator or type (op=" + operation.toString() + " type=" + common_type->toString() + ")");
}

std::shared_ptr<meta::Type> Binary::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  auto lhs_type = throwIfNull(lhs->generateType(ctx, {}), CodegenException("LHS type is NULL"));
  auto rhs_type = throwIfNull(rhs->generateType(ctx, {}), CodegenException("RHS type is NULL"));

  return meta::Type::alignTypes(lhs_type, rhs_type);
}

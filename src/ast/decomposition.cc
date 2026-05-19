#include "xcc/ast/decomposition.h"
#include "xcc/codegen.h"
#include "xcc/error.h"

using namespace xcc;
using namespace xcc::ast;

Decomposition::Decomposition(
  SourceSpan            span,
  NodeList              pieces,
  std::shared_ptr<Node> value
) : Node(AST_DECOMPOSITION_DECL, span), pieces(std::move(pieces)), value(std::move(value)) {}

std::shared_ptr<Decomposition> Decomposition::create(
  SourceSpan            span,
  NodeList              pieces,
  std::shared_ptr<Node> value
) {
  return std::make_shared<Decomposition>(span, std::move(pieces), std::move(value));
}

std::shared_ptr<Node> Decomposition::clone() {
  return withAttrs(create(span, cloneVector(pieces), value->clone()));
}

void Decomposition::visit(std::unique_ptr<codegen::GlobalContext>& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  for (auto& piece : pieces) {
    callVisitor(globalContext, piece, visitor, ignoreSubtree);
  }

  callVisitor(globalContext, value, visitor, ignoreSubtree);
}

std::string Decomposition::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = parent->is(AST_DECOMPOSITION_DECL) ? "[" : "var [";

  for (size_t i = 0; i < pieces.size(); ++i) {
    res += pieces[i]->toString(parent, this, indent, false);
    if (i + 1 < pieces.size()) {
      res += ", ";
    }
  }

  return res + "]" + (value ? " = " + value->toString(parent, this, indent, false) : "");
}

llvm::Value * Decomposition::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  ctx.setDebugLocation(span);

  auto base = value->generateValue(ctx, payload);
  auto type = value->generateType(ctx, payload);

  decomposeListPiece(ctx, payload, base, type, pieces);

  return base;
}

std::shared_ptr<meta::Type> Decomposition::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  return value ? value->generateType(ctx, payload) : nullptr;
}

void Decomposition::decomposeListPiece(
  codegen::ModuleContext&            ctx,
  PayloadList                        payload,
  llvm::Value *                      base_val,
  std::shared_ptr<meta::Type>        base_type,
  NodeList sub_pieces
) {
  for (size_t i = 0; i < sub_pieces.size(); ++i) {
    auto& piece = sub_pieces[i];

    if (piece->is(AST_EXPR_IDENTIFIER) && piece->as<Identifier>()->value == "_") {
      assertRaiseFromNode(i == sub_pieces.size() - 1, Error(ERROR_DECOMPOSITION_BAD_WILDCARD, piece->span), this);
      return;
    }

    llvm::Value *               val      = nullptr;
    std::shared_ptr<meta::Type> val_type = generateTypeForPiece(base_type, i);

    if (!base_val->getType()->isPointerTy()) {
      // Tuples and structs are registers (rvalues) - extract directly
      val = ctx.ir_builder->CreateExtractValue(
          base_val,
          static_cast<unsigned>(i),
          "decomposition_elem"
      );
    } else if (base_type->is(meta::TypeTag::ARRAY)) {
      // Arrays are pointers (lvalues), calculate address via GEP and load
      std::vector<llvm::Value *> indices = {
        ctx.ir_builder->getInt32(0),
        ctx.ir_builder->getInt32(static_cast<uint32_t>(i))
      };

      auto * element_ptr = ctx.ir_builder->CreateInBoundsGEP(
        base_type->getLLVMType(ctx), // Underlying array type
        base_val,                    // Alloca pointer
        indices,
        "array_decomp_ptr"
      );

      val = ctx.ir_builder->CreateLoad(
        val_type->getLLVMType(ctx),  // Inner element type
        element_ptr,
        "decomposition_elem"
      );
    } else {
      Error(ERROR_DECOMPOSITION_BAD_TYPE, value->span, "'{}'", base_type->toString()).raiseFromNode(this);
    }

    if (piece->is(AST_EXPR_IDENTIFIER)) {
      decomposeNamedPiece(ctx, payload, piece->as<Identifier>()->value, val, val_type);
    } else if (piece->is(AST_DECOMPOSITION_DECL)) {
      decomposeListPiece(ctx, payload, val, val_type, piece->as<Decomposition>()->pieces);
    } else {
      Error(ERROR_INTERNAL_UNEXPECTED_NULL, piece->span, "'{}'", typeToHumanReadableString(piece->type)).raiseFromNode(this);
    }
  }
}

std::shared_ptr<meta::Type> Decomposition::generateTypeForPiece(
  std::shared_ptr<meta::Type> base_type,
  size_t                      pieceIndex
) {
  assertRaiseFromNode(pieceIndex < pieces.size(),
    Error(ERROR_INTERNAL_OUT_OF_BOUNDS, span, "'{}' is out of bounds for decomposition with {} pieces", pieceIndex, pieces.size()), this);

  if (base_type->is(meta::TypeTag::TUPLE)) {
    return base_type->getTupleMemberType(pieceIndex);
  }

  if (base_type->is(meta::TypeTag::STRUCT)) {
    return base_type->getMemberType(base_type->getMemberName(pieceIndex));
  }

  if (base_type->is(meta::TypeTag::ARRAY)) {
    return base_type->getElementType();
  }

  Error(ERROR_DECOMPOSITION_BAD_TYPE, value->span, "'{}'", base_type->toString()).raiseFromNode(this);
}

void Decomposition::decomposeNamedPiece(
  codegen::ModuleContext&     ctx,
  PayloadList                 payload,
  const std::string&          name,
  llvm::Value *               val,
  std::shared_ptr<meta::Type> val_type
) {
  auto fn = ctx.ir_builder->GetInsertBlock()->getParent();
  auto tv = meta::TypedValue::create(ctx, fn, span, val_type, name);

  auto di_local = ctx.globalContext.di_builder->createAutoVariable(
      ctx.currentDIScope(),
      name,
      ctx.globalContext.getCurrentDIFile(),
      span.start().line,
      val_type->getDIType(ctx),
      true
    );

  ctx.globalContext.di_builder->insertDeclare(
    tv->value,
    di_local,
    ctx.globalContext.di_builder->createExpression(),
    span.start().getDILocation(ctx),
    ctx.ir_builder->GetInsertBlock()
  );

  ctx.ir_builder->CreateStore(val, tv->value);

  ctx.addLocal(name, tv);
}

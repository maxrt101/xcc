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

Assign::Assign(SourceSpan span, Token kind, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
  : Node(AST_EXPR_ASSIGN, span), kind(std::move(kind)), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

std::shared_ptr<Assign> Assign::create(SourceSpan span, Token kind, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs) {
  return std::make_shared<Assign>(span, std::move(kind), std::move(lhs), std::move(rhs));
}

std::shared_ptr<Node> Assign::clone() {
  return withAttrs(create(span, kind, lhs->clone(), rhs->clone()));
}

void Assign::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(lhs, visitor, ignoreSubtree);
  callVisitor(rhs, visitor, ignoreSubtree);
}

std::string Assign::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return attributesToString(0, false) + std::format("{} = {}",
    lhs->toString(parent, this, indent, false),
    rhs->toString(parent, this, indent, false)
  );
}

llvm::Value * Assign::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  llvm::Value * value = nullptr;

  if (kind.type == TOKEN_EQUALS) {
    value = rhs->generateValue(ctx, {});
  } else if (s_equal_to_op.find(kind.type) != s_equal_to_op.end()) {
    value = Binary::create(span, kind.clone(s_equal_to_op[kind.type]), lhs, rhs)->generateValue(ctx, payload);
  } else {
    Error(ERROR_INVALID_ASSIGNMENT_OP, kind.span, "{}", Token::typeToString(kind.type)).raiseFromNode(this);
  }

  value = codegen::castIfNotSame(
    ctx,
    throwIfNull(value, std::runtime_error("assignment value generated NULL")),
    lhs->generateTypeForValueWithoutLoad(ctx, {})->getLLVMType(ctx),
    span
  );

  ctx.ir_builder->CreateStore(
    value,
    lhs->generateValueWithoutLoad(ctx, payload)
  );

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

  return lhs->generateTypeForValueWithoutLoad(ctx, {});
}

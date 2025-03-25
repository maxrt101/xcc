#include "xcc/ast/assign.h"
#include "xcc/ast/number.h"
#include "xcc/ast/binary.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

using namespace xcc::ast;
using namespace xcc;

static std::unordered_map<TokenType, TokenType> s_equal_to_op = {
  {TOKEN_ADD_EQUALS, TOKEN_PLUS},
  {TOKEN_MIN_EQUALS, TOKEN_MINUS},
  {TOKEN_MUL_EQUALS, TOKEN_STAR},
  {TOKEN_DIV_EQUALS, TOKEN_SLASH},
  {TOKEN_AND_EQUALS, TOKEN_AMP},
  {TOKEN_OR_EQUALS, TOKEN_VERTICAL_LINE},
  {TOKEN_LOGICAL_AND_EQUALS, TOKEN_AND},
  {TOKEN_LOGICAL_OR_EQUALS, TOKEN_OR},
};

Assign::Assign(Token kind, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
  : Node(AST_EXPR_ASSIGN), kind(std::move(kind)), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

std::shared_ptr<Assign> Assign::create(Token kind, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs) {
  return std::make_shared<Assign>(std::move(kind), std::move(lhs), std::move(rhs));
}

llvm::Value * Assign::generateValue(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  llvm::Value * value = nullptr;

  if (kind.type == TOKEN_EQUALS) {
    value = rhs->generateValue(ctx, {});
  } else if (s_equal_to_op.find(kind.type) != s_equal_to_op.end()) {
    value = Binary::create(kind.clone(s_equal_to_op[kind.type]), lhs, rhs)->generateValue(ctx, payload);
  } else {
    throw CodegenException("Invalid operation for assignment (" + Token::typeToString(kind.type) + ")");
  }

  value = codegen::castIfNotSame(
    ctx,
    throwIfNull(value, CodegenException("assignment value generated NULL")),
    lhs->generateTypeForValueWithoutLoad(ctx, {})->getLLVMType(ctx)
  );

  return ctx.ir_builder->CreateStore(
    value,
    lhs->generateValueWithoutLoad(ctx, payload)
  );
}

std::shared_ptr<xcc::meta::Type> Assign::generateType(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  if (lhs->is(ast::AST_EXPR_IDENTIFIER)) {
    auto name = ast::Node::cast<ast::Identifier>(lhs);

    if (ctx.hasLocal(name->value)) {
      return ctx.getLocalType(name->value);
    }

    throw CodegenException("Unknown variable '" + name->value + "'");
  } else {
    return lhs->generateTypeForValueWithoutLoad(ctx, {});
  }
}

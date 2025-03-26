#include "xcc/ast/while.h"
#include "xcc/exceptions.h"
#include "xcc/codegen.h"

using namespace xcc::ast;

While::While(std::shared_ptr<Node> condition, std::shared_ptr<Node> body)
  : Node(AST_WHILE), condition(std::move(condition)), body(std::move(body)) {}

std::shared_ptr<While> While::create(std::shared_ptr<Node> condition, std::shared_ptr<Node> body) {
  return std::make_shared<While>(std::move(condition), std::move(body));
}

llvm::Value * While::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  throw CodegenException("while loops are unsupported");
}

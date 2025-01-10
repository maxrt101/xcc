#include "xcc/ast/node.h"
#include "xcc/codegen.h"
#include "xcc/util/log.h"
#include "xcc/exceptions.h"
#include <unordered_map>

using namespace xcc;
using namespace xcc::ast;

static auto logger = xcc::util::log::Logger("AST_NODE");

Node::Payload::Payload(NodeType type) : type(type) {}

Node::Node(NodeType type) : type(type) {}

std::vector<std::shared_ptr<Node::Payload>> Node::selectPayload(const std::vector<std::shared_ptr<Node::Payload>>& payload) {
  std::vector<std::shared_ptr<Node::Payload>> result;

  for (const auto& element : payload) {
    if (element->type == type) {
      result.push_back(element);
    }
  }

  return result;
}

std::shared_ptr<Node::Payload> Node::selectPayloadFirst(const std::vector<std::shared_ptr<Node::Payload>>& payload) {
  for (const auto& element : payload) {
    if (element->type == type) {
      return element;
    }
  }

  return {};
}

llvm::Value * Node::generateValue(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  logger.warn("Warning: Default Node::generateValue is called on node with type '%s' (%d)", Node::typeToString(type).c_str(), int(type));
  return nullptr;
}

llvm::Value * Node::generateValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  return generateValue(ctx, std::move(payload));
}

llvm::Function * Node::generateFunction(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  logger.warn("Warning: Default Node::generateFunction is called on node with type '%s' (%d)", Node::typeToString(type).c_str(), int(type));
  return nullptr;
}

std::shared_ptr<meta::Type> Node::generateType(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  logger.warn("Warning: Default Node::generateType is called on node with type '%s' (%d)", Node::typeToString(type).c_str(), int(type));
  return nullptr;
}

std::shared_ptr<meta::Type> Node::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  return generateType(ctx, std::move(payload));
}

std::string Node::typeToString(NodeType type) {
  static std::unordered_map<NodeType, std::string> type_map {
      {AST_EXPR_NUMBER, "AST_EXPR_NUMBER"},
      {AST_EXPR_STRING, "AST_EXPR_STRING"},
      {AST_EXPR_IDENTIFIER, "AST_EXPR_IDENTIFIER"},
      {AST_EXPR_CALL, "AST_EXPR_CALL"},
      {AST_EXPR_CAST, "AST_EXPR_CAST"},
      {AST_EXPR_BINARY, "AST_EXPR_BINARY"},
      {AST_EXPR_UNARY, "AST_EXPR_UNARY"},
      {AST_EXPR_ASSIGN, "AST_EXPR_ASSIGN"},
      {AST_EXPR_TYPE, "AST_EXPR_TYPE"},
      {AST_EXPR_TYPED_IDENTIFIER, "AST_EXPR_TYPED_IDENTIFIER"},
      {AST_BLOCK, "AST_BLOCK"},
      {AST_VAR_DECL, "AST_VAR_DECL"},
      {AST_FUNCTION_DECL, "AST_FUNCTION_DECL"},
      {AST_FUNCTION_DEF, "AST_FUNCTION_DEF"},
      {AST_IF, "AST_IF"},
      {AST_FOR, "AST_FOR"},
      {AST_WHILE, "AST_WHILE"},
      {AST_RETURN, "AST_RETURN"},
  };

  if (type_map.find(type) != type_map.end()) {
    return type_map[type];
  }

  return "UNKNOWN";
}

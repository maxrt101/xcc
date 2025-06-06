#include "xcc/ast/node.h"
#include "xcc/codegen.h"
#include "xcc/util/log.h"
#include "xcc/exceptions.h"
#include <unordered_map>

using namespace xcc;
using namespace xcc::ast;

static auto logger = xcc::util::log::Logger("AST_NODE");

static const std::unordered_map<NodeType, std::string> s_type_map {
    {AST_EXPR_ASSIGN,           "AST_EXPR_ASSIGN"},
    {AST_EXPR_BINARY,           "AST_EXPR_BINARY"},
    {AST_BLOCK,                 "AST_BLOCK"},
    {AST_EXPR_CALL,             "AST_EXPR_CALL"},
    {AST_EXPR_CAST,             "AST_EXPR_CAST"},
    {AST_FUNCTION_DECL,         "AST_FUNCTION_DECL"},
    {AST_FUNCTION_DEF,          "AST_FUNCTION_DEF"},
    {AST_FOR,                   "AST_FOR"},
    {AST_EXPR_IDENTIFIER,       "AST_EXPR_IDENTIFIER"},
    {AST_IF,                    "AST_IF"},
    {AST_EXPR_MEMBER_ACCESS,    "AST_EXPR_MEMBER_ACCESS"},
    {AST_EXPR_NUMBER,           "AST_EXPR_NUMBER"},
    {AST_RETURN,                "AST_RETURN"},
    {AST_EXPR_STRING,           "AST_EXPR_STRING"},
    {AST_STRUCT,                "AST_STRUCT"},
    {AST_EXPR_SUBSCRIPT,        "AST_EXPR_SUBSCRIPT"},
    {AST_EXPR_TYPE,             "AST_EXPR_TYPE"},
    {AST_EXPR_TYPED_IDENTIFIER, "AST_EXPR_TYPED_IDENTIFIER"},
    {AST_EXPR_UNARY,            "AST_EXPR_UNARY"},
    {AST_VAR_DECL,              "AST_VAR_DECL"},
    {AST_WHILE,                 "AST_WHILE"},
};

Node::Payload::Payload(NodeType type) : type(type) {}

Node::Node(NodeType type) : type(type) {}

Node::PayloadList Node::selectPayload(const PayloadList& payload) {
  PayloadList result;

  for (const auto& element : payload) {
    if (element->type == type) {
      result.push_back(element);
    }
  }

  return result;
}

std::shared_ptr<Node::Payload> Node::selectPayloadFirst(const PayloadList& payload) {
  for (const auto& element : payload) {
    if (element->type == type) {
      return element;
    }
  }

  return {};
}

llvm::Function * Node::generateFunction(codegen::ModuleContext& ctx, PayloadList payload) {
  logger.warn("Warning: Default Node::generateFunction is called on node with type '{}' ({})", Node::typeToString(type).c_str(), int(type));
  return nullptr;
}

llvm::Value * Node::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  logger.warn("Warning: Default Node::generateValue is called on node with type '{}' ({})", Node::typeToString(type).c_str(), int(type));
  return nullptr;
}

llvm::Value * Node::generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  return generateValue(ctx, std::move(payload));
}

std::shared_ptr<meta::Type> Node::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  logger.warn("Warning: Default Node::generateType is called on node with type '{}' ({})", Node::typeToString(type).c_str(), int(type));
  return nullptr;
}

std::shared_ptr<meta::Type> Node::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  return generateType(ctx, std::move(payload));
}

std::string Node::typeToString(NodeType type) {
  if (s_type_map.find(type) != s_type_map.end()) {
    return s_type_map.at(type);
  }

  return "UNKNOWN";
}

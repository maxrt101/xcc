#pragma once

#include <string>

#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

namespace xcc::codegen {
class ModuleContext;
}

namespace xcc::meta {
class Type;
class Function;
}

namespace xcc::ast {

enum NodeType {
  AST_EXPR_NUMBER,            // [0-9]+
  AST_EXPR_STRING,            // ".+"
  AST_EXPR_IDENTIFIER,        // [a-zA-Z_][a-zA-Z0-9_]+ - id

  AST_EXPR_CALL,              // id (expr, ...)
  AST_EXPR_CAST,              // expr as type
  AST_EXPR_BINARY,            // lhs op rhs
  AST_EXPR_UNARY,             // op rhs
  AST_EXPR_SUBSCRIPT,         // lhs [ rhs ]
  AST_EXPR_MEMBER_ACCESS,     // lhs . rhs
  AST_EXPR_ASSIGN,            // if = expr

  AST_EXPR_TYPE,              // identifier (generic?)
  AST_EXPR_TYPED_IDENTIFIER,  // name: type [= value]

  AST_BLOCK,                  // { expr; ... }
  AST_VAR_DECL,               // var name: type [= value]
  AST_FUNCTION_DECL,          // fn name([args])[: type];
  AST_FUNCTION_DEF,           // function-decl { body }
  AST_STRUCT,                 // struct name { field: type [= init], ... }
  AST_IF,                     // if (cond) then else
  AST_FOR,                    // for (init; cond; inc) body | for (typed_id in expr) body
  AST_WHILE,                  // while (cond) body
  AST_RETURN,                 // return expr
};

class Node {
public:
  struct Payload {
    NodeType type;

    Payload(NodeType type);
    virtual ~Payload() = default;

    template <typename T>
    inline T * as() {
      return (T *)this;
    }
  };

public:
  NodeType type;

public:
  explicit Node(NodeType type);
  virtual ~Node() = default;

  template <typename T>
  inline T* as() {
    return (T*)this;
  }

  inline bool is(NodeType expected) const {
    return type == expected;
  }

  template <typename ...Types>
  inline bool isAnyOf(Types... expected) const {
    return ((this->type == expected) || ...);
  }

  template <typename T>
  static inline std::shared_ptr<T> cast(std::shared_ptr<Node>& ptr) {
    return std::dynamic_pointer_cast<T>(ptr);
  }

  template <typename T>
  static inline std::shared_ptr<Node> cast(std::shared_ptr<T>& ptr) {
    return std::dynamic_pointer_cast<Node>(ptr);
  }

  std::vector<std::shared_ptr<Node::Payload>> selectPayload(const std::vector<std::shared_ptr<Node::Payload>>& payload);
  std::shared_ptr<Node::Payload> selectPayloadFirst(const std::vector<std::shared_ptr<Node::Payload>>& payload);

  virtual llvm::Value * generateValue(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload);
  virtual llvm::Value * generateValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload);
  virtual llvm::Function * generateFunction(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload);
  virtual std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload);
  virtual std::shared_ptr<meta::Type> generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload);

  static std::string typeToString(NodeType type);
};

} /* namespace xcc::ast */
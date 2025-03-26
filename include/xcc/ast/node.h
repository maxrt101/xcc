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

/**
 * Abstract Syntax Tree Node Type
 */
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

/**
 * Abstract Syntax Tree Node
 */
class Node {
public:
  /**
   * Custom payload parent class for generate* virtual member functions
   *
   * Can be inherited and extended to pass special data that
   * will be passed along the chain of generate* functions
   *
   * Use selectPayload/selectPayloadFirst from within any ast node to
   * retrieve payload for current node
   */
  struct Payload {
    /**
     * For which node the payload is intended
     */
    NodeType type;

    explicit Payload(NodeType type);
    virtual ~Payload() = default;

    /**
     * Convenience pointer cast
     *
     * Example:
     * @code{.c}
     *   class SpecificPayload : public Node::Payload { ... };
     *   Node::Payload * payload = new SpecificPayload(...);
     *   payload->as<SpecificPayload>()->value;
     * @encode
     *
     * @tparam T Class to convert generic payload into
     */
    template <typename T>
    T * as() {
      return (T *)this;
    }
  };

  /**
   * Shortcut for vector of shared pointers to generic payload
   */
  using PayloadList = std::vector<std::shared_ptr<Payload>>;

public:
  NodeType type;

public:
  explicit Node(NodeType type);
  virtual ~Node() = default;

  /**
   * Convenience pointer cast
   *
   * Example:
   * @code{.c}
   *   class SpecificNode : public Node { ... };
   *   Node * node = new SpecificNode(...);
   *   node->as<SpecificNode>()->value;
   * @encode
   *
   * @tparam T Class to convert generic node into
   */
  template <typename T>
  T* as() {
    return (T*)this;
  }

  /**
   * Checks if current node is of `expected` type
   *
   * @param expected Node type to check against current node's type
   */
  bool is(NodeType expected) const {
    return type == expected;
  }

  /**
   * Checks if current node is any of `expected` types
   *
   * @param expected Node type list against current node's type
   */
  template <typename ...Types>
  bool isAnyOf(Types... expected) const {
    return ((this->type == expected) || ...);
  }

  /**
   * Cast shared pointer to generic node into shared pointer to specific node
   *
   * Used when node is needed to be of another type, but refcnt should be kept
   * (sort of like sharing ownership of the same data, interpreted differently)
   *
   * Example:
   * @code{.c}
   *   class SpecificNode : public Node { ... };
   *   std::shared_ptr<Node> node = new SpecificNode::create(...);
   *   std::shared_ptr<SpecificNode> spec_node = Node::cast<SpecificNode>(node);
   * @encode
   *
   * @tparam T Node type to cast `ptr` into
   * @param ptr Pointer to generic node
   */
  template <typename T>
  static std::shared_ptr<T> cast(std::shared_ptr<Node>& ptr) {
    return std::dynamic_pointer_cast<T>(ptr);
  }

  /**
   * Cast shared pointer to specific node into shared pointer to generic node
   *
   * Reverse of cast() a bit higher
   *
   * Used when node is needed to be of another type, but refcnt should be kept
   * (sort of like sharing ownership of the same data, interpreted differently)
   *
   * Example:
   * @code{.c}
   *   class SpecificNode : public Node { ... };
   *   std::shared_ptr<SpecificNode> spec_node = new SpecificNode::create(...);
   *   std::shared_ptr<Node> node = Node::cast(spec_node);
   * @encode
   *
   * @tparam T Node type
   * @param ptr Pointer to node
   */
  template <typename T>
  static std::shared_ptr<Node> cast(std::shared_ptr<T>& ptr) {
    return std::dynamic_pointer_cast<Node>(ptr);
  }

  /**
   * Select payload intended for current node from payload list
   *
   * @param payload Payload list
   * @return filtered payload in regard to this->type
   */
  PayloadList selectPayload(const PayloadList& payload);

  /**
   * Select first occurrence of payload intended for current node from payload list
   *
   * @param payload Payload list
   * @return first of filtered payload in regard to this->type
   */
  std::shared_ptr<Node::Payload> selectPayloadFirst(const PayloadList& payload);

  /**
   * Generates llvm::Function from node
   *
   * Part of codegen API. Implemented when node has a relation to functions (fndecl/fndef/etc.)
   *
   * @param ctx Module Context, holds a lot of state, needed to work with llvm APIs
   * @param payload custom payload
   */
  virtual llvm::Function * generateFunction(codegen::ModuleContext& ctx, PayloadList payload);

  /**
   * Generates llvm::Value from node
   *
   * Part of codegen API. Implemented when node has a relation to variables/expressions that can
   * evaluate to a value
   *
   * @param ctx Module Context, holds a lot of state, needed to work with llvm APIs
   * @param payload custom payload
   */
  virtual llvm::Value * generateValue(codegen::ModuleContext& ctx, PayloadList payload);

  /**
   * Generates llvm::Value from node without generating instruction to actually load that value
   *
   * Basically load a reference/pointer to a value, without loading its value
   * Useful, for example in assignment, when LHS hods an lvalue into which RHS should be stored,
   * so actual value of LHS shouldn't be loaded, only it's address
   *
   * Part of codegen API. Implemented when node has a relation to variables/expressions that can
   * evaluate to a memory location
   *
   * @param ctx Module Context, holds a lot of state, needed to work with llvm APIs
   * @param payload custom payload
   */
  virtual llvm::Value * generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload);

  /**
   * Generates (meta) type that llvm::Value return from generateValue will have
   *
   * Part of codegen API. Implemented when node has a relation to variables/expressions that can
   * evaluate to a value with a type
   *
   * @param ctx Module Context, holds a lot of state, needed to work with llvm APIs
   * @param payload custom payload
   */
  virtual std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload);

  /**
   * Generates (meta) type that llvm::Value return from generateValueWithoutLoad will have
   *
   * Part of codegen API. Implemented when node has a relation to variables/expressions that can
   * evaluate to a pointer to a value with a type
   *
   * @param ctx Module Context, holds a lot of state, needed to work with llvm APIs
   * @param payload custom payload
   */
  virtual std::shared_ptr<meta::Type> generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload);

  /**
   * Converts node type to its string representation
   *
   * @param type Node type
   */
  static std::string typeToString(NodeType type);
};

} /* namespace xcc::ast */
#pragma once

#include "xcc/error.h"

#include <string>
#include <functional>

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
  AST_EMPTY = 0,

  AST_EXPR_NUMBER,            // [0-9]+
  AST_EXPR_STRING,            // ".+"
  AST_EXPR_IDENTIFIER,        // [a-zA-Z_][a-zA-Z0-9_]+ [:: id] - id

  AST_EXPR_CALL,              // id (expr, ...)
  AST_EXPR_MACRO_CALL,        // id ! (expr, ...)
  AST_EXPR_CAST,              // expr as type
  AST_EXPR_BINARY,            // lhs op rhs
  AST_EXPR_UNARY,             // op rhs
  AST_EXPR_SUBSCRIPT,         // lhs [ rhs ]
  AST_EXPR_MEMBER_ACCESS,     // lhs . rhs
  AST_EXPR_ASSIGN,            // if = expr

  AST_EXPR_TYPE,              // identifier (generic?)
  AST_EXPR_TYPED_IDENTIFIER,  // name: type [= value]

  AST_BLOCK,                  // { expr; ... }
  AST_VAR_DECL,               // var name [: type] [= value]
  AST_CONST_DECL,             // const name [: type] [= value]
  AST_DECOMPOSITION_DECL,     // var [id, id, _] = expr
  AST_FUNCTION_DECL,          // fn name([id: type, ...])[: type];
  AST_FUNCTION_DEF,           // function-decl { body }
  AST_TYPE_DECL,              // type id = type_expr
  AST_STRUCT,                 // struct name { field: type [= init], ... }
  AST_IF,                     // if (cond) then else
  AST_FOR,                    // for (init; cond; inc) body | for (typed_id in expr) body
  AST_WHILE,                  // while (cond) body
  AST_RETURN,                 // return expr

  AST_INIT,                   // type { id: expr, ... } OR type { expr, ... }

  AST_MOD,                    // module
  AST_MACRO,                  // macro id (id, ...) block

  AST_ASM,                    // asm!(...)
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

  /**
   * An attribute. Can be applied to any node (technically), for now
   * applies only to top-level nodes (fn, var, use, mod)
   *
   * Has a syntax of "[attr(arg1, ...), attr2, ...]"
   */
  struct Attribute {
    std::string                        name;
    std::vector<std::shared_ptr<Node>> args;
    SourceSpan                         span;

    /**
     * Validates `args`. Checks count & NodeTypes. Throws an exception on failure
     *
     * @param arg_types Ordered types of arguments
     */
    void validateArgsStrict(const std::vector<NodeType>& arg_types);

    /**
     * Validates `args`. Checks count & NodeTypes. Throws an exception on failure
     *
     * @param arg_types Ordered possible types of arguments
     */
    void validateArgs(const std::vector<std::vector<NodeType>>& arg_types);
  };

  /**
   * Shortcut for a vector of Attributes
   */
  using AttributeList = std::vector<Attribute>;

  /**
   * Visitor functor
   *
   * If node needs to be modified, return new node, if not - return nullptr
   */
  using Visitor = std::function<std::shared_ptr<Node>(std::shared_ptr<Node>)>;

public:
  NodeType      type;
  AttributeList attributes;
  SourceSpan    span;

public:
  explicit Node(NodeType type, SourceSpan span);
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
  bool isAnyOf(std::vector<NodeType> expected) const;

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
  static std::shared_ptr<T> cast(std::shared_ptr<Node> ptr) {
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
  static std::shared_ptr<Node> cast(std::shared_ptr<T> ptr) {
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
  std::shared_ptr<Payload> selectPayloadFirst(const PayloadList& payload);

  /**
   * Extends provided payload list with new value
   *
   * @param list    Payload list to extend
   * @param payload New payload
   * @return `list` with appended `payload`
   */
  static PayloadList extendPayload(PayloadList list, std::shared_ptr<Payload> payload);

  /**
   * Excludes payload for provided node from list
   *
   * @param list List of payload to filter
   * @param type Node type, payload for which must be excluded
   * @return Filtered `list`
   */
  static PayloadList excludePayload(PayloadList list, NodeType type);

  /**
   * Add attribute to the attribute list
   *
   * @param attr Attribute
   */
  void addAttribute(const Attribute& attr);

  /**
   * Check if node has a specific attribute
   *
   * @param name Attribute name
   * @return @c true if attribute with specified name is present
   */
  bool hasAttribute(const std::string& name) const;

  /**
   * Get attribute by name
   *
   * @note Will throw if attribute is missing
   *
   * @param name Attribute name
   * @return Attribute reference
   */
  Attribute& getAttribute(const std::string& name);

  /**
   * Get attribute by name
   *
   * @note Will throw if attribute is missing
   *
   * @param name Attribute name
   * @return Const attribute reference
   */
  const Attribute& getAttribute(const std::string& name) const;

  /**
   * Get all attributes where Attribute::name == `name` as references
   */
  std::vector<std::reference_wrapper<Attribute>> getAttributes(const std::string& name);

  /**
   * Get all attributes where Attribute::name == `name` as const references
   */
  std::vector<std::reference_wrapper<const Attribute>> getAttributes(const std::string& name) const;

  /**
   * Perform a clone (deep copy) of current node;
   *
   * @return Identical node to current
   */
  virtual std::shared_ptr<Node> clone() = 0;

  /**
   * Visitor implementation
   */
  virtual void visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) = 0;

  /**
   * Recursively convert tree to string
   *
   * @param grandparent Grandparent node (for root - can be nullptr)
   * @param parent      Parent node (for root - can be nullptr)
   * @param indent      Indentation level
   * @param newline     Should print new lines
   */
  virtual std::string toString(Node * grandparent, Node * parent, int indent, bool newline) = 0;

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
   * @param ctx     Module Context, holds a lot of state, needed to work with llvm APIs
   * @param payload Custom payload
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
   * @param payload Custom payload
   */
  virtual llvm::Value * generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload);

  /**
   * Generates llvm::Constant from node
   *
   * Part of codegen API. Implemented when node's value can be evaluated at compile time for
   * global variable initialization
   *
   * @param ctx     Module Context, holds a lot of state, needed to work with llvm APIs
   * @param payload Custom payload
   */
  virtual llvm::Constant * generateConstant(codegen::ModuleContext& ctx, PayloadList payload);

  /**
   * Generates (meta) type that llvm::Value return from generateValue will have
   *
   * Part of codegen API. Implemented when node has a relation to variables/expressions that can
   * evaluate to a value with a type
   *
   * @param ctx     Module Context, holds a lot of state, needed to work with llvm APIs
   * @param payload Custom payload
   */
  virtual std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload);

  /**
   * Generates (meta) type that llvm::Value return from generateValueWithoutLoad will have
   *
   * Part of codegen API. Implemented when node has a relation to variables/expressions that can
   * evaluate to a pointer to a value with a type
   *
   * @param ctx     Module Context, holds a lot of state, needed to work with llvm APIs
   * @param payload Custom payload
   */
  virtual std::shared_ptr<meta::Type> generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload);

  /**
   * Converts node type to its string representation
   *
   * @param type Node type
   */
  static std::string typeToString(NodeType type);

  /**
   * Converts node type to its human-readable string representation
   *
   * @param type Node type
   */
  static std::string typeToHumanReadableString(NodeType type);

  /**
   * Used to generate indent in toString
   *
   * @param indent Intent level (not in chars)
   */
  static std::string getIndent(int indent);

  /**
   * Convert current Node's AttributeList to string
   */
  std::string attributesToString(int indent, bool newline);

  /**
   * Clone a vector of nodes
   */
  template <typename T>
  [[nodiscard]] static std::vector<std::shared_ptr<T>> cloneVector(std::vector<std::shared_ptr<T>> nodes) {
    std::vector<std::shared_ptr<T>> cloned;

    for (auto& node : nodes) {
      cloned.push_back(cast<T>(node->clone()));
    }

    return cloned;
  }

protected:
  /**
   * Add attributes from this to node & return node;
   */
  template <typename T>
  [[nodiscard]] std::shared_ptr<T> withAttrs(std::shared_ptr<T> node) const {
    node->attributes = attributes;
    return node;
  }

  template <typename T>
  void callVisitor(std::shared_ptr<T>& node, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
    if (!node || node->isAnyOf(ignoreSubtree)) return;

    node->visit(visitor, ignoreSubtree);

    auto res = visitor(cast<Node>(node));

    if (res) {
      node = cast<T>(res);
    }
  }
};

/**
 * Empty AST node. May be used as a placeholder in the tree
 */
class Empty : public Node {
public:
  Empty();
  ~Empty() override = default;

  static std::shared_ptr<Empty> create();

  std::shared_ptr<Node> clone() override;
  void visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;
};

} /* namespace xcc::ast */
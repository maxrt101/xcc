#pragma once

#include "xcc/ast/node.h"
#include "xcc/meta/function.h"

#include <string>
#include <vector>

namespace xcc::ast {

class Type : public Node {
public:
  enum Kind {
    NORMAL,
    POINTER,
    ARRAY,
    FUNCTION,
    LAMBDA,
    TUPLE,
  };

  // Type kind
  Kind kind;

  // Type name (empty for everything except Kind::NORMAL)
  std::shared_ptr<Node> name;

  // For Kind::ARRAY
  struct {
    std::shared_ptr<Node> size;
  } array;

  // For Kind::FUNCTION
  struct {
    std::shared_ptr<Node> returnType;
    NodeList              args;
    bool                  isVariadic;
  } fn;

  // For Kind::LAMBDA
  struct {
    NodeList captures;
  } lambda;

  // For Kind::TUPLE
  struct {
    NodeList members;
  } tuple;

public:
  Type(SourceSpan span, LexicalScope scope, Kind kind, std::shared_ptr<Node> name);
  ~Type() override = default;

  static std::shared_ptr<Type> create(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> name);
  static std::shared_ptr<Type> createPointer(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> name);
  static std::shared_ptr<Type> createArray(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> name, std::shared_ptr<Node> size);
  static std::shared_ptr<Type> createFunction(SourceSpan span, LexicalScope scope, std::shared_ptr<Node> returnType, NodeList args, bool isVariadic = false);
  static std::shared_ptr<Type> createLambda(SourceSpan span, LexicalScope scope, NodeList captures, std::shared_ptr<Node> returnType, NodeList args, bool isVariadic = false);
  static std::shared_ptr<Type> createTuple(SourceSpan span, LexicalScope scope, NodeList members);

  std::shared_ptr<Node> clone() override;
  void visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;

  std::shared_ptr<meta::Type> getBaseType(codegen::ModuleContext& ctx, PayloadList payload);
};

} /* namespace xcc::ast */

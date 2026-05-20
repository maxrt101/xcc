#pragma once

#include "xcc/ast/node.h"

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

  Kind kind;

  std::shared_ptr<Node> name;

  struct {
    std::shared_ptr<Node> size;
  } array;

  struct {
    std::shared_ptr<Node> returnType;
    NodeList              args;
    bool                  isVariadic;
  } fn;

  struct {
    NodeList              captures;
  } lambda;

  struct {
    NodeList members;
  } tuple;

public:
  Type(SourceSpan span, Kind kind, std::shared_ptr<Node> name);
  ~Type() override = default;

  static std::shared_ptr<Type> create(SourceSpan span, std::shared_ptr<Node> name);
  static std::shared_ptr<Type> createPointer(SourceSpan span, std::shared_ptr<Node> name);
  static std::shared_ptr<Type> createArray(SourceSpan span, std::shared_ptr<Node> name, std::shared_ptr<Node> size);
  static std::shared_ptr<Type> createFunction(SourceSpan span, std::shared_ptr<Node> returnType, NodeList args, bool isVariadic = false);
  static std::shared_ptr<Type> createLambda(SourceSpan span, NodeList captures, std::shared_ptr<Node> returnType, NodeList args, bool isVariadic = false);
  static std::shared_ptr<Type> createTuple(SourceSpan span, NodeList members);

  std::shared_ptr<Node> clone() override;
  void visit(std::unique_ptr<codegen::GlobalContext>& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;

  std::shared_ptr<meta::Type> getBaseType(codegen::ModuleContext& ctx, PayloadList payload);
};

} /* namespace xcc::ast */

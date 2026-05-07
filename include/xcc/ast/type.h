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
  };

  Kind kind;

  std::shared_ptr<Node> name;

  struct {
    std::shared_ptr<Node> size;
  } array;

  struct {
    std::shared_ptr<Node>              returnType;
    std::vector<std::shared_ptr<Node>> args;
    bool                               isVariadic;
  } fn;

public:
  Type(SourceSpan span, Kind kind, std::shared_ptr<Node> name);
  ~Type() override = default;

  static std::shared_ptr<Type> create(SourceSpan span, std::shared_ptr<Node> name);
  static std::shared_ptr<Type> createPointer(SourceSpan span, std::shared_ptr<Node> name);
  static std::shared_ptr<Type> createArray(SourceSpan span, std::shared_ptr<Node> name, std::shared_ptr<Node> size);
  static std::shared_ptr<Type> createFunction(SourceSpan span, std::shared_ptr<Node> returnType, std::vector<std::shared_ptr<Node>> args, bool isVariadic = false);

  std::shared_ptr<Node> clone() override;
  void visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, PayloadList payload) override;

  std::shared_ptr<meta::Type> getBaseType(codegen::ModuleContext& ctx, PayloadList payload);
};

} /* namespace xcc::ast */

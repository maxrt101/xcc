#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/fndef.h"
#include "xcc/ast/typed_identifier.h"

#include <string>
#include <vector>

namespace xcc::ast {

class Struct : public Node, public std::enable_shared_from_this<Struct> {
public:
  std::shared_ptr<Identifier>                   name;
  std::vector<std::shared_ptr<TypedIdentifier>> fields;
  std::vector<std::shared_ptr<Node>>            methods;

public:
  explicit Struct(
      SourceSpan                                    span,
      std::shared_ptr<Identifier>                   name,
      std::vector<std::shared_ptr<TypedIdentifier>> fields = {},
      std::vector<std::shared_ptr<Node>>            methods = {}
  );

  ~Struct() override = default;

  static std::shared_ptr<Struct> create(
      SourceSpan                                    span,
      std::shared_ptr<Identifier>                   name,
      std::vector<std::shared_ptr<TypedIdentifier>> fields = {},
      std::vector<std::shared_ptr<Node>>            methods = {}
  );

  std::shared_ptr<Node> clone() override;
  void visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  std::shared_ptr<xcc::meta::Type> generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext &ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

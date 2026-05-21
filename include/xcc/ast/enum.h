#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/fndef.h"
#include "xcc/ast/typed_identifier.h"

#include <string>
#include <vector>

namespace xcc::ast {

class Enum : public Node {
public:
  using FieldList = std::vector<std::pair<std::shared_ptr<Identifier>, std::shared_ptr<Node>>>;

  std::shared_ptr<Identifier> name;
  std::shared_ptr<Node>       type;
  FieldList                   fields;
  NodeList                    methods;

public:
  explicit Enum(
      SourceSpan                  span,
      std::shared_ptr<Identifier> name,
      std::shared_ptr<Node>       type,
      FieldList                   fields = {},
      NodeList                    methods = {}
  );

  ~Enum() override = default;

  static std::shared_ptr<Enum> create(
      SourceSpan                  span,
      std::shared_ptr<Identifier> name,
      std::shared_ptr<Node>       type,
      FieldList                   fields = {},
      NodeList                    methods = {}
  );

  std::shared_ptr<Node> clone() override;
  void visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  std::shared_ptr<meta::Type> generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext &ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

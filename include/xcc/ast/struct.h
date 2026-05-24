#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/fndef.h"
#include "xcc/ast/typed_identifier.h"

#include <string>
#include <vector>

namespace xcc::ast {

class Struct : public Node, public std::enable_shared_from_this<Struct> {
public:
  struct GenericParam {
    std::shared_ptr<Node> name;
    std::shared_ptr<Node> default_value;
  };

  std::shared_ptr<Identifier>                   name;
  std::vector<GenericParam>                     genericParams;
  std::vector<std::shared_ptr<TypedIdentifier>> fields;
  NodeList                                      methods;

public:
  Struct(
      SourceSpan                                    span,
      LexicalScope                                  scope,
      std::shared_ptr<Identifier>                   name,
      std::vector<GenericParam>                     genericParams = {},
      std::vector<std::shared_ptr<TypedIdentifier>> fields       = {},
      NodeList                                      methods      = {}
  );

  ~Struct() override = default;

  static std::shared_ptr<Struct> create(
      SourceSpan                                    span,
      LexicalScope                                  scope,
      std::shared_ptr<Identifier>                   name,
      std::vector<GenericParam>                     genericParams = {},
      std::vector<std::shared_ptr<TypedIdentifier>> fields       = {},
      NodeList                                      methods      = {}
  );

  bool isGeneric() const;
  NodeList getGenericParamNames() const;

  std::shared_ptr<Node> clone() override;
  void visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;

  std::shared_ptr<meta::Type> generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) override;
  std::shared_ptr<meta::Type> generateType(codegen::ModuleContext &ctx, PayloadList payload) override;
};

} /* namespace xcc::ast */

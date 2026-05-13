#include "xcc/ast/struct.h"
#include "xcc/codegen.h"

using namespace xcc;
using namespace xcc::ast;

Struct::Struct(
    SourceSpan                                    span,
    std::shared_ptr<Identifier>                   name,
    std::vector<std::shared_ptr<TypedIdentifier>> fields,
    std::vector<std::shared_ptr<Node>>            methods
) : Node(AST_STRUCT, span),
    name(std::move(name)),
    fields(std::move(fields)),
    methods(std::move(methods)) {}

std::shared_ptr<Struct> Struct::create(
    SourceSpan                                    span,
    std::shared_ptr<Identifier>                   name,
    std::vector<std::shared_ptr<TypedIdentifier>> fields,
    std::vector<std::shared_ptr<Node>>            methods
) {
  return std::make_shared<Struct>(span, std::move(name), std::move(fields), std::move(methods));
}

std::shared_ptr<Node> Struct::clone() {
  return withAttrs(create(span, cast<Identifier>(name->clone()), cloneVector(fields), cloneVector(methods)));
}

void Struct::visit(Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(name, visitor, ignoreSubtree);

  for (auto& node : fields) {
    callVisitor(node, visitor, ignoreSubtree);
  }

  for (auto& node : methods) {
    callVisitor(node, visitor, ignoreSubtree);
  }
}

std::string Struct::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res =attributesToString(indent, newline) +  std::format("struct {} {{",
    name->toString(parent, this, indent, false)
  );

  res += newline ? "\n" : " ";

  for (auto& field : fields) {
    res += newline ? getIndent(indent + 1) : " ";
    res += field->toString(parent, this, indent, false);
    res += ";";
    res += newline ? "\n" : " ";
  }

  for (auto& method : methods) {
    res += newline ? getIndent(indent + 1) : " ";
    res += method->toString(parent, this, indent, newline);
    res += newline ? "\n" : " ";
  }

  if (newline) {
    res += getIndent(indent) + "}\n";
  } else {
    res += "}";
  }

  return res;
}

std::shared_ptr<meta::Type> Struct::generateType(codegen::ModuleContext &ctx, PayloadList payload) {
  return generateTypeForValueWithoutLoad(ctx, payload);
}

std::shared_ptr<meta::Type> Struct::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  bool packed = false;

  if (hasAttribute("packed")) {
    packed = true;
  }

  auto type = meta::Type::createStruct(name->name(), {}, packed);

  // Create an empty struct type, to allow self-referential types
  meta::Type::registerCustomType(name->name(), type);

  for (auto & field : fields) {
    type->addMember(field->name->name(), field->generateType(ctx, payload));
  }

  return type;
}

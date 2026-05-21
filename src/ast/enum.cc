#include "xcc/ast/enum.h"
#include "xcc/codegen.h"

using namespace xcc;
using namespace xcc::ast;

Enum::Enum(
  SourceSpan                  span,
  std::shared_ptr<Identifier> name,
  std::shared_ptr<Node>       type,
  FieldList                   fields,
  NodeList                    methods
) : Node(AST_ENUM, span),
    name(std::move(name)),
    type(std::move(type)),
    fields(std::move(fields)),
    methods(std::move(methods)) {}

std::shared_ptr<Enum> Enum::create(
  SourceSpan                  span,
  std::shared_ptr<Identifier> name,
  std::shared_ptr<Node>       type,
  FieldList                   fields,
  NodeList                    methods
) {
  return std::make_shared<Enum>(span, std::move(name), std::move(type), std::move(fields), std::move(methods));
}

std::shared_ptr<Node> Enum::clone() {
  FieldList fields;

  for (auto& f : this->fields) {
    fields.emplace_back(cast<Identifier>(f.first->clone()), f.second->clone());
  }

  return withAttrs(create(span, cast<Identifier>(name->clone()), type->clone(), fields, cloneVector(methods)));
}

void Enum::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(globalContext, name, visitor, ignoreSubtree);
  callVisitor(globalContext, type, visitor, ignoreSubtree);

  for (auto& f : fields) {
    callVisitor(globalContext, f.first, visitor, ignoreSubtree);
    callVisitor(globalContext, f.second, visitor, ignoreSubtree);
  }

  for (auto& node : methods) {
    callVisitor(globalContext, node, visitor, ignoreSubtree);
  }
}

std::string Enum::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(indent, newline) +  std::format("enum {} : {} {{",
    name->toString(parent, this, indent, false),
    type->toString(parent, this, indent, false)
  );

  res += newline ? "\n" : " ";

  for (auto& field : fields) {
    res += newline ? getIndent(indent + 1) : " ";
    res += field.first->toString(parent, this, indent, false);
    if (field.second) {
      res += " = ";
      res += field.second->toString(parent, this, indent, false);
    }
    res += ",";
    res += newline ? "\n" : " ";
  }

  for (auto& method : methods) {
    res += newline ? getIndent(indent + 1) : " ";
    res += method->toString(parent, this, indent + 1, newline);
    res += newline ? "\n" : " ";
  }

  if (newline) {
    res += getIndent(indent) + "}\n";
  } else {
    res += "}";
  }

  return res;
}

std::shared_ptr<meta::Type> Enum::generateType(codegen::ModuleContext &ctx, PayloadList payload) {
  return generateTypeForValueWithoutLoad(ctx, payload);
}

std::shared_ptr<meta::Type> Enum::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  auto id = name->name();

  if (meta::Type::hasCustomType(id)) {
    return meta::Type::getCustomType(id);
  }

  auto t = meta::Type::createEnum(id, type->generateType(ctx, payload), {});

  // Create an empty enum type, to allow self-referential values
  meta::Type::registerCustomType(id, t);

  meta::EnumValue last = 0;

  for (auto & field : fields) {
    meta::EnumValue value;

    if (field.second) {
      if (auto * const_int = llvm::dyn_cast<llvm::ConstantInt>(field.second->generateConstant(ctx, payload))) {
        value = const_int->getSExtValue();
        last = value + 1;
      } else {
        Error(ERROR_NOT_CONSTANT, field.second->span, "Enum field values must evaluate to a constant integer")
          .raiseFromNode(this);
      }
    } else {
      value = last++;
    }

    t->addEnumElement({field.first->value, value});
  }

  return t;
}

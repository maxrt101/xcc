#include "xcc/ast/struct.h"
#include "xcc/codegen.h"
#include "xcc/util/log.h"

using namespace xcc;
using namespace xcc::ast;

Struct::Struct(
    SourceSpan                                    span,
    LexicalScope                                  scope,
    std::shared_ptr<Identifier>                   name,
    std::vector<GenericParam>                     genericParams,
    std::vector<std::shared_ptr<TypedIdentifier>> fields,
    NodeList                                      methods
) : Node(AST_STRUCT, span, scope),
    name(std::move(name)),
    genericParams(std::move(genericParams)),
    fields(std::move(fields)),
    methods(std::move(methods)) {}

std::shared_ptr<Struct> Struct::create(
    SourceSpan                                    span,
    LexicalScope                                  scope,
    std::shared_ptr<Identifier>                   name,
    std::vector<GenericParam>                     genericParams,
    std::vector<std::shared_ptr<TypedIdentifier>> fields,
    NodeList                                      methods
) {
  return std::make_shared<Struct>(
    span, scope,
    std::move(name),
    std::move(genericParams),
    std::move(fields),
    std::move(methods)
  );
}

bool Struct::isGeneric() const {
  return !genericParams.empty();
}

NodeList Struct::getGenericParamNames() const {
  NodeList result;

  for (auto & param : genericParams) {
    result.push_back(param.name);
  }

  return result;
}

std::shared_ptr<Node> Struct::clone() {
  std::vector<GenericParam> generics;

  for (auto& g : genericParams) {
    generics.push_back({g.name->clone(), g.default_value ? g.default_value->clone() : nullptr});
  }

  return withAttrs(create(span, scope, cast<Identifier>(name->clone()), generics, cloneVector(fields), cloneVector(methods)));
}

void Struct::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(globalContext, name, visitor, ignoreSubtree);

  for (auto& node : fields) {
    callVisitor(globalContext, node, visitor, ignoreSubtree);
  }

  for (auto& node : methods) {
    callVisitor(globalContext, node, visitor, ignoreSubtree);
  }
}

std::string Struct::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string generics;

  if (!genericParams.empty()) {
    generics += "<";
    for (size_t i = 0; i < genericParams.size(); ++i) {
      generics += genericParams[i].name->toString(parent, this, indent, false);
      if (genericParams[i].default_value) {
        generics += " = " + genericParams[i].default_value->toString(parent, this, indent, false);
      }
      if (i + 1 < genericParams.size()) {
        generics += ", ";
      }
    }
    generics += ">";
  }

  std::string res = attributesToString(indent, newline) +  std::format("struct {}{} {{",
    generics,
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

std::shared_ptr<meta::Type> Struct::generateType(codegen::ModuleContext &ctx, PayloadList payload) {
  return generateTypeForValueWithoutLoad(ctx, payload);
}

std::shared_ptr<meta::Type> Struct::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  auto id = getMangledName(name->value);

  std::shared_ptr<meta::Type> type;

  if (meta::Type::hasCustomType(id)) {
    return meta::Type::getCustomType(id);
  }

  std::string dropMethodName;

  for (auto& method : methods) {
    if (method->hasAttribute("drop")) {
      auto decl = method->is(AST_FUNCTION_DEF) ? method->as<FnDef>()->decl.get() : method->as<FnDecl>();
      dropMethodName = id + "_" + decl->name->as<Identifier>()->value;
    }
  }

  type = meta::Type::createStruct(id, {}, hasAttribute("packed"), dropMethodName);

  // Create an empty struct type, to allow self-referential types
  meta::Type::registerCustomType(id, type);

  for (auto & field : fields) {
    type->addMember(field->name->name(), field->generateType(ctx, payload));
  }

  return type;
}

void Struct::generateForwardDeclarations(codegen::ModuleContext& ctx, PayloadList payload) {
  for (auto& method : methods) {
    auto decl = method->is(AST_FUNCTION_DEF) ? method->as<FnDef>()->decl : cast<FnDecl>(method);

    decl->generateFunction(ctx, payload);
  }
}

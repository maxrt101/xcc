#include "xcc/ast/struct.h"
#include "xcc/codegen.h"

using namespace xcc;
using namespace xcc::ast;

Struct::Struct(
    std::shared_ptr<Identifier> name,
    std::vector<std::shared_ptr<TypedIdentifier>> fields
) : Node(AST_STRUCT),
    name(std::move(name)),
    fields(std::move(fields)) {}

std::shared_ptr<Struct> Struct::create(
    std::shared_ptr<Identifier> name,
    std::vector<std::shared_ptr<TypedIdentifier>> fields
) {
  return std::make_shared<Struct>(std::move(name), std::move(fields));
}


std::shared_ptr<xcc::meta::Type> Struct::generateType(codegen::ModuleContext &ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  return generateTypeForValueWithoutLoad(ctx, payload);
}

std::shared_ptr<xcc::meta::Type> Struct::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) {
  meta::StructMembers members;

  for (auto & field : fields) {
    members.emplace_back(field->name->value, field->generateType(ctx, {}));
  }

  auto type =  meta::Type::createStruct(members);

  meta::Type::registerCustomType(name->value, type);

  return type;
}

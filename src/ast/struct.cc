#include "xcc/ast/struct.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"

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
  std::unordered_map<std::string, std::shared_ptr<meta::Type>> meta_elements;

  for (auto & field : fields) {
    meta_elements[field->name->value] = field->generateType(ctx, {});
  }

  auto type =  meta::Type::createStruct(meta_elements);

  meta::Type::registerCustomType(name->value, type);

  return type;
}

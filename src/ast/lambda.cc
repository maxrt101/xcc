#include "xcc/ast/lambda.h"

using namespace xcc;
using namespace xcc::ast;

Lambda::Lambda(
    SourceSpan                                    span,
    NodeList                                      captures,
    std::vector<std::shared_ptr<TypedIdentifier>> args,
    std::shared_ptr<Node>                         return_type,
    std::shared_ptr<Node>                         body,
    bool                                          isVariadic
) : Node(AST_LAMBDA, span),
    captures(std::move(captures)),
    args(std::move(args)),
    return_type(std::move(return_type)),
    body(std::move(body)),
    isVariadic(isVariadic) {}

std::shared_ptr<Lambda> Lambda::create(
    SourceSpan                                    span,
    NodeList                                      captures,
    std::vector<std::shared_ptr<TypedIdentifier>> args,
    std::shared_ptr<Node>                         return_type,
    std::shared_ptr<Node>                         body,
    bool                                          isVariadic
) {
  return std::make_shared<Lambda>(span, std::move(captures), std::move(args), std::move(return_type), std::move(body), isVariadic);
}

std::shared_ptr<Node> Lambda::clone() {
  return withAttrs(create(
    span,
    cloneVector(captures),
    cloneVector(args),
    return_type->clone(),
    body->clone(),
    isVariadic
  ));
}

void Lambda::visit(std::unique_ptr<codegen::GlobalContext>& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  for (auto& node : captures) {
    callVisitor(globalContext, node, visitor, ignoreSubtree);
  }

  for (auto& node : args) {
    callVisitor(globalContext, node, visitor, ignoreSubtree);
  }

  callVisitor(globalContext, return_type, visitor, ignoreSubtree);
  callVisitor(globalContext, body, visitor, ignoreSubtree);
}

std::string Lambda::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = attributesToString(indent, newline) + "fn [";

  for (size_t i = 0; i < captures.size(); ++i) {
    res += captures[i]->toString(parent, this, indent, false);

    if (i + 1 < captures.size()) {
      res += ", ";
    }
  }

  res += "] (";

  for (size_t i = 0; i < args.size(); ++i) {
    res += args[i]->toString(parent, this, indent, false);

    if (i + 1 < args.size()) {
      res += ", ";
    }
  }

  if (isVariadic) {
    res += ", ...";
  }

  res += std::format(") -> {}", return_type->toString(parent, this, indent, false));
  res += body->toString(parent, this, indent + 1, newline);

  return res;
}

llvm::Value * Lambda::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  Error(ERROR_UNIMPLEMENTED, span, "Lambdas are in progress").raise();
}

std::shared_ptr<meta::Type> Lambda::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  Error(ERROR_UNIMPLEMENTED, span, "Lambdas are in progress").raise();
}

std::shared_ptr<meta::Type> Lambda::generateType(codegen::ModuleContext &ctx, PayloadList payload) {
  Error(ERROR_UNIMPLEMENTED, span, "Lambdas are in progress").raise();
}

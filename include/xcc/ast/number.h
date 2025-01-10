#pragma once

#include "xcc/ast/node.h"

namespace xcc::ast {

class Number : public Node {
public:
  struct Payload : public Node::Payload {
    int bits;

    Payload(int bits);
    ~Payload() override = default;

    static std::shared_ptr<Node::Payload> create(int bits);
  };

public:
  enum {
    INTEGER,
    FLOATING
  } tag;

  union {
    int64_t integer;
    double floating;
  } value;

public:
  Number();
  virtual ~Number() override = default;

  static std::shared_ptr<Number> createInteger(int64_t value);
  static std::shared_ptr<Number> createFloating(double value);

  llvm::Value * generateValue(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
  llvm::Value * generateValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
  std::shared_ptr<xcc::meta::Type> generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, std::vector<std::shared_ptr<Node::Payload>> payload) override;
};

} /* namespace xcc::ast */

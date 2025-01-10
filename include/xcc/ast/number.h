#pragma once

#include "xcc/ast/node.h"

namespace xcc::ast {

class Number : public Node {
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

  // FIXME: Find another way
  llvm::Value * generateValueWithSpecificBitWidth(codegen::ModuleContext& ctx, int bits) const;

  llvm::Value * generateValue(codegen::ModuleContext& ctx) override;
  llvm::Value * generateValueWithoutLoad(codegen::ModuleContext& ctx) override;
  std::shared_ptr<xcc::meta::Type> generateType(codegen::ModuleContext& ctx) override;
};

} /* namespace xcc::ast */

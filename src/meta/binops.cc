#include "xcc/meta/binops.h"
#include "xcc/util/string.h"

using namespace xcc::BinaryOperationConditions;

static const std::unordered_map<uint8_t, std::string> s_binop_cond_str_map = {
  {INTEGER, "INTEGER"},
  {FLOAT, "FLOAT"},
  {SIGNED, "SIGNED"},
  {UNSIGNED, "UNSIGNED"},
};

bool xcc::BinaryOperationMeta::check(const BinaryOperationMeta& rhs) const {
  if (op != rhs.op) {
    return false;
  }

  if (!cond) {
    return true;
  }

  if (cond & INTEGER && rhs.cond & INTEGER) {
    return cond & SIGNED   ? rhs.cond & SIGNED   :
           cond & UNSIGNED ? rhs.cond & UNSIGNED :
           true;

  }

  return cond & rhs.cond;
}

std::string xcc::BinaryOperationMeta::toString() const {
  if (!cond) {
    return "NONE";
  }

  std::vector<std::string> result;

  for (auto & p : s_binop_cond_str_map) {
    if (cond & p.first) {
      result.push_back(p.second);
    }
  }

  return Token::typeToString(op) + "(" + util::strjoin(result) + ")";
}

xcc::BinaryOperationMeta xcc::BinaryOperationMeta::fromType(TokenType op, std::shared_ptr<meta::Type> type) {
  return {op,
    (uint8_t) (
      (type->isInteger() ? INTEGER : 0) |
      (type->isFloat() ? FLOAT : 0) |
      (type->isSigned() ? SIGNED : 0) |
      (type->isUnsigned() ? UNSIGNED : 0)
    )
  };
}

const xcc::BinaryOperation * xcc::findBinaryOperation(const BinaryOperations& binops, const BinaryOperationMeta& meta) {
  for (size_t i = 0; i < binops.size(); ++i) {
    if (binops[i].meta.check(meta)) {
      return &binops[i];
    }
  }

  return nullptr;
}

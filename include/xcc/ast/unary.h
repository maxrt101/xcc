#pragma once

#include "xcc/ast/node.h"
#include "xcc/lexer.h"

#include <string>

namespace xcc::ast {

class Unary : public Node {
public:
  Token operation;
  std::shared_ptr<Node> rhs;

public:
  Unary(Token operation, std::shared_ptr<Node> rhs);
  virtual ~Unary() override = default;

  static std::shared_ptr<Unary> create(Token operation, std::shared_ptr<Node> rhs);
};

} /* namespace xcc::ast */

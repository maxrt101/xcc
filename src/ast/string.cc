#include "xcc/ast/string.h"

using namespace xcc::ast;

String::String(const std::string& value) : Node(AST_EXPR_STRING), value(value) {}

std::shared_ptr<String> String::create(const std::string& value) {
  return std::make_shared<String>(value);
}

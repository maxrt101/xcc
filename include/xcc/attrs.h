#pragma once

#include <functional>
#include <string>

#include "xcc/ast.h"

namespace xcc::attr {

using Handler = std::function<void(const ast::Node::Attribute&, ast::Node*)>;

void registerHandler(const std::string& name, Handler handler);
void callHandler(const ast::Node::Attribute& attr, ast::Node * node);

// TODO: loadHandlers(.so)

} /* namespace xcc::attr */

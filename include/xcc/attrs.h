#pragma once

#include <functional>
#include <string>

#include "codegen.h"
#include "xcc/ast.h"

namespace xcc::attr {

using Handler = std::function<void(codegen::GlobalContext&, const ast::Node::Attribute&, ast::Node*)>;

void registerHandler(const std::string& name, Handler handler);
void callHandler(codegen::GlobalContext& globalContext, const ast::Node::Attribute& attr, ast::Node * node);

// TODO: loadHandlers(.so)

} /* namespace xcc::attr */

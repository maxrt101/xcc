#pragma once

#include "xcc/ast/asm.h"
#include "xcc/ast/assign.h"
#include "xcc/ast/binary.h"
#include "xcc/ast/block.h"
#include "xcc/ast/call.h"
#include "xcc/ast/cast.h"
#include "xcc/ast/const.h"
#include "xcc/ast/decomposition.h"
#include "xcc/ast/enum.h"
#include "xcc/ast/fndecl.h"
#include "xcc/ast/fndef.h"
#include "xcc/ast/for.h"
#include "xcc/ast/identifier.h"
#include "xcc/ast/if.h"
#include "xcc/ast/init.h"
#include "xcc/ast/macro.h"
#include "xcc/ast/macro_call.h"
#include "xcc/ast/member.h"
#include "xcc/ast/node.h"
#include "xcc/ast/number.h"
#include "xcc/ast/return.h"
#include "xcc/ast/string.h"
#include "xcc/ast/struct.h"
#include "xcc/ast/subscript.h"
#include "xcc/ast/type.h"
#include "xcc/ast/typed_identifier.h"
#include "xcc/ast/typedecl.h"
#include "xcc/ast/unary.h"
#include "xcc/ast/mod.h"
#include "xcc/ast/vardecl.h"
#include "xcc/ast/while.h"

namespace xcc::ast {

/**
 * Return true if `node` is of `type` type,
 * or if `node` is a block - if last node in block is of `type` type
 */
bool isOrIsLastInBlock(std::shared_ptr<Node> node, NodeType type);

/**
 * Returns `node`, if it is of type `type`
 * Returns last child of `node`, if the node is of type AST_BLOCK and
 * child is of type `type`
 * Returns nullptr if none of the conditions above are met
 */
std::shared_ptr<Node> getOrGetLastInBlock(std::shared_ptr<Node> node, NodeType type);

/**
 * Returns `note` if it is not a block, recursively searches for last node in block, and returns it otherwise
 */
std::shared_ptr<Node> getOrGetLastInBlock(std::shared_ptr<Node> node);

namespace subtree {

/**
 * Recursively replace identifier with node
 */
void replaceIdentifierWithNode(const std::shared_ptr<Node>& node, const std::string& oldValue, std::shared_ptr<Node> newNode);

/**
 * Recursively replace value of identifiers
 */
void replaceIdentifier(const std::shared_ptr<Node>& node, const std::string& oldValue, const std::string& newValue);

}

}

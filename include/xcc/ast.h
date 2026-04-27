#pragma once

#include "xcc/ast/assign.h"
#include "xcc/ast/binary.h"
#include "xcc/ast/block.h"
#include "xcc/ast/call.h"
#include "xcc/ast/cast.h"
#include "xcc/ast/fndecl.h"
#include "xcc/ast/fndef.h"
#include "xcc/ast/for.h"
#include "xcc/ast/identifier.h"
#include "xcc/ast/if.h"
#include "xcc/ast/member.h"
#include "xcc/ast/node.h"
#include "xcc/ast/number.h"
#include "xcc/ast/return.h"
#include "xcc/ast/string.h"
#include "xcc/ast/struct.h"
#include "xcc/ast/subscript.h"
#include "xcc/ast/type.h"
#include "xcc/ast/typed_identifier.h"
#include "xcc/ast/unary.h"
#include "xcc/ast/mod.h"
#include "xcc/ast/vardecl.h"
#include "xcc/ast/while.h"

namespace xcc::ast {

/**
 * Recursive node printer. Used by printAst
 *
 * @warning For internal use only
 *
 * @param node   Node to print
 * @param parent Parent of `node`
 * @param indent Indentation level
 */
void printNode(Node* node, Node* parent, int indent);

/**
 * Pretty print an AST
 *
 * @param root Node to print
 */
void printAst(std::shared_ptr<Node> root);

/**
 * Return true if `node` is of `type` type,
 * or if `node` is a block - if last node in block is of `type` type
 */
bool isOrIsLastInBlock(std::shared_ptr<Node> node, NodeType type);

}

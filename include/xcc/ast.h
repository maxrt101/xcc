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
#include "xcc/ast/lambda.h"
#include "xcc/ast/macro.h"
#include "xcc/ast/macro_call.h"
#include "xcc/ast/match.h"
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

namespace xcc::codegen {
class GlobalContext;
}

namespace xcc::ast {

/**
 * Class that implements generic monomorphization, namely replaces all occurances of generalized named with concrete ones
 */
class Monomorphizer {
public:
  codegen::GlobalContext& globalContext;

  /**
   * baseName - Unqualified generalized name (e.g. 'Container' for 'test::Container<T>')
   * genericName - Qualified generalized name (e.g. 'test_Container' for 'test::Container<T>')
   * concreteName - Qualified specific/concrete name (e.g. 'test_Container_i32' for 'test::Container<i32>')
   * concreteUnqualifiedName - Unqualified specific/concrete name (e.g. 'Container_i32' for 'test::Container<i32>')
   */
  std::string baseName, genericName, concreteName, concreteUnqualifiedName;

  /// Map of generic param name ('T') to concrete type name ('i32')
  std::unordered_map<std::string, std::shared_ptr<Node>> substitutions;

  Monomorphizer(
    codegen::GlobalContext& ctx,
    const std::string&      baseName,
    const std::string&      genericName,
    const std::string&      concreteName,
    const std::string&      concreteUnqualifiedName,
    const NodeList&         params,
    const NodeList&         args
  );

  /**
   * Performs replacement of generic names to concrete ones
   *
   * @param node Struct definition. Should be deep-copied via Node::clone from generic template
   */
  void apply(const std::shared_ptr<Node>& node);
};

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

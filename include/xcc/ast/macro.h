#pragma once

#include "xcc/ast/node.h"
#include "xcc/ast/block.h"
#include "xcc/ast/identifier.h"
#include "xcc/ast/macro_call.h"

#include <functional>
#include <unordered_map>

namespace xcc::codegen
{
class GlobalContext;
}

namespace xcc::ast {

class Macro : public Node {
public:
  using NativeFn = std::function<std::shared_ptr<Node>(codegen::GlobalContext&, std::shared_ptr<MacroCall>&)>;

  std::shared_ptr<Identifier>              name;
  std::vector<std::shared_ptr<Identifier>> args;
  std::shared_ptr<Block>                   body;

  bool     native;
  NativeFn fn;

  bool variadic;

public:
  Macro(
    SourceSpan                               span,
    std::shared_ptr<Identifier>              name,
    std::vector<std::shared_ptr<Identifier>> args,
    std::shared_ptr<Block>                   body
  );

  Macro(
    std::shared_ptr<Identifier>              name,
    std::vector<std::shared_ptr<Identifier>> args,
    NativeFn                                 fn,
    bool                                     variadic
  );

  ~Macro() override = default;

  static std::shared_ptr<Macro> create(
    SourceSpan                               span,
    std::shared_ptr<Identifier>              name,
    std::vector<std::shared_ptr<Identifier>> args,
    std::shared_ptr<Block>                   body
  );

  static std::shared_ptr<Macro> createNative(
    std::shared_ptr<Identifier>              name,
    std::vector<std::shared_ptr<Identifier>> args,
    NativeFn                                 fn,
    bool                                     variadic
  );

  std::shared_ptr<Node> clone() override;
  void visit(std::unique_ptr<codegen::GlobalContext>& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;
};

} /* namespace xcc::ast */

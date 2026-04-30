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
  struct NativeContext {
    codegen::GlobalContext&                                global;
    std::unordered_map<std::string, std::shared_ptr<Node>> vardecls;
    std::unordered_map<std::string, std::shared_ptr<Node>> fndecls;
    std::unordered_map<std::string, std::shared_ptr<Node>> args;
  };

  using NativeFn = std::function<std::shared_ptr<Node>(NativeContext&, std::shared_ptr<MacroCall>)>;

  std::shared_ptr<Identifier>              name;
  std::vector<std::shared_ptr<Identifier>> args;
  std::shared_ptr<Block>                   body;

  bool     native;
  NativeFn fn;

public:
  Macro(
    std::shared_ptr<Identifier>              name,
    std::vector<std::shared_ptr<Identifier>> args,
    std::shared_ptr<Block>                   body
  );

  Macro(
    std::shared_ptr<Identifier>              name,
    std::vector<std::shared_ptr<Identifier>> args,
    NativeFn                                 fn
  );

  ~Macro() override = default;

  static std::shared_ptr<Macro> create(
    std::shared_ptr<Identifier>              name,
    std::vector<std::shared_ptr<Identifier>> args,
    std::shared_ptr<Block>                   body
  );

  static std::shared_ptr<Macro> createNative(
    std::shared_ptr<Identifier>              name,
    std::vector<std::shared_ptr<Identifier>> args,
    NativeFn                                 fn
  );

  std::shared_ptr<Node> clone() override;
  void visit(Visitor visitor) override;
  std::string toString(Node * grandparent, Node * parent, int indent, bool newline) override;
};

} /* namespace xcc::ast */

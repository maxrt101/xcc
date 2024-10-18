#pragma once

#include "xcc/meta/type.h"
#include "xcc/ast/fndecl.h"
#include "xcc/util/ordered_map.h"

#include <map>

namespace xcc::codegen {
class ModuleContext;
}

namespace xcc::meta {

class Function {
public:
  std::string name;
  std::shared_ptr<Type> returnType;

  OrderedMap<std::string, std::shared_ptr<Type>> args;

  std::shared_ptr<ast::FnDecl> decl;

public:
  Function(std::string name, std::shared_ptr<Type> returnType, OrderedMap<std::string, std::shared_ptr<Type>> args = {}, std::shared_ptr<ast::FnDecl> decl = nullptr);
  ~Function() = default;

  static std::shared_ptr<Function> create(std::string name, std::shared_ptr<Type> returnType, OrderedMap<std::string, std::shared_ptr<Type>> args = {}, std::shared_ptr<ast::FnDecl> decl = nullptr);

  llvm::Type * getLLVMReturnType(codegen::ModuleContext& ctx);
  std::vector<llvm::Type *> getLLVMArgTypes(codegen::ModuleContext& ctx);

  std::string toString() const;

  static std::vector<llvm::Type *> typesFromMetaArgs(codegen::ModuleContext& ctx, OrderedMap<std::string, std::shared_ptr<Type>> args);
};

}

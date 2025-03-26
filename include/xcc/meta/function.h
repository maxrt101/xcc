#pragma once

#include "xcc/meta/type.h"
#include "xcc/ast/fndecl.h"
#include "xcc/util/ordered_map.h"

#include <map>

namespace xcc::codegen {
class ModuleContext;
}

namespace xcc::meta {

/**
 * Function metadata
 */
class Function {
public:
  /** Function name */
  std::string name;

  /** Function return type */
  std::shared_ptr<Type> returnType;

  /** Function argument pairs (name - (meta) type) */
  OrderedMap<std::string, std::shared_ptr<Type>> args;

  /** Function declaration node for future use */
  std::shared_ptr<ast::FnDecl> decl;

public:
  Function(std::string name, std::shared_ptr<Type> returnType, OrderedMap<std::string, std::shared_ptr<Type>> args = {}, std::shared_ptr<ast::FnDecl> decl = nullptr);
  ~Function() = default;

  static std::shared_ptr<Function> create(std::string name, std::shared_ptr<Type> returnType, OrderedMap<std::string, std::shared_ptr<Type>> args = {}, std::shared_ptr<ast::FnDecl> decl = nullptr);

  /**
   * Generate llvm::Type* from function return type metadata. Needs ModuleContext
   */
  llvm::Type * getLLVMReturnType(codegen::ModuleContext& ctx);

  /**
   * Generate list of llvm::Type* from function argument types metadata
   */
  std::vector<llvm::Type *> getLLVMArgTypes(codegen::ModuleContext& ctx);

  /**
   * Returns pretty string for a function declaration based on metadata
   */
  std::string toString() const;

  /**
   * Returns list of llvm::Type* from list of argument types metadata, doesn't require a meta::Function object
   */
  static std::vector<llvm::Type *> typesFromMetaArgs(codegen::ModuleContext& ctx, OrderedMap<std::string, std::shared_ptr<Type>> args);
};

}

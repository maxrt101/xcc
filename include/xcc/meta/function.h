#pragma once

#include "xcc/meta/type.h"
#include "xcc/util/ordered_map.h"

#include <map>

namespace xcc::codegen {
class ModuleContext;
}

namespace xcc::ast {
struct FnDecl;
}

namespace xcc::meta {

using SubstitutionMap = std::unordered_map<std::string, std::shared_ptr<Type>>;

/**
 * Function metadata
 */
class Function {
public:
  /** Function name */
  std::string name;

  /** What this function aliases */
  std::string alias_to;

  /** Function return type */
  std::shared_ptr<Type> returnType;

  /** Function argument pairs (name - (meta) type) */
  OrderedMap<std::string, std::shared_ptr<Type>> args;

  /** Function declaration node for future use */
  std::shared_ptr<ast::FnDecl> decl;

  /** Substitutions for generic methods */
  SubstitutionMap substitutions;

public:
  Function(
    std::string                                    name,
    std::shared_ptr<Type>                          returnType,
    OrderedMap<std::string, std::shared_ptr<Type>> args          = {},
    std::shared_ptr<ast::FnDecl>                   decl          = nullptr,
    SubstitutionMap                                substitutions = {}
  );

  ~Function() = default;

  static std::shared_ptr<Function> create(
    std::string                                    name,
    std::shared_ptr<Type>                          returnType,
    OrderedMap<std::string, std::shared_ptr<Type>> args          = {},
    std::shared_ptr<ast::FnDecl>                   decl          = nullptr,
    SubstitutionMap                                substitutions = {}
  );

  /**
   * Generate llvm Function from declaration, using caches substitutions, if they are provided
   */
  llvm::Function * generateFunction(codegen::ModuleContext& ctx);

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

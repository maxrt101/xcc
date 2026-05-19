#pragma once

#include <string>
#include <cstdint>
#include "xcc/lexer.h"
#include "xcc/parser.h"
#include "xcc/codegen.h"
#include "xcc/exceptions.h"
#include "xcc/util/filemng.h"
#include "xcc/util/llvm.h"
#include "xcc/util/log.h"

namespace xcc {

constexpr uint8_t XCC_VERSION_MAJOR = 0;
constexpr uint8_t XCC_VERSION_MINOR = 1;
constexpr uint8_t XCC_VERSION_PATCH = 0;

/**
 * Returns symver version string in format major.minor.patch
 */
inline std::string getVersion() {
  return std::to_string(XCC_VERSION_MAJOR) + "."
       + std::to_string(XCC_VERSION_MINOR) + "."
       + std::to_string(XCC_VERSION_PATCH);
}

/**
 * Compilation result, lists all declared functions and/or top-level expressions. Used in REPL/JIT runs
 */
struct CompilationResult {
  struct {
    ast::NodeList expr;
  } nodes;
};

/**
 * Initializes compiler & LLVM
 *
 * @param target      Target architecture to initialize LLVM for
 * @param machine     Target machine (cpu) to initialize LLVM for
 * @param autoCleanup if true will set atexit callback with xcc::cleanup function
 */
util::Target init(const std::string& target, const std::string& machine, bool autoCleanup = true);

/**
 * Deinitializes compiler & LLVM
 *
 * @note If `autoCleanup` it true in @ref xcc::init - this shouldn't be run manually
 */
void cleanup();

/**
 * 'Driver' function - will tokenize, parse, lower AST & compile the source code
 *
 * @param globalContext Global context
 * @param file          ID of loaded file
 * @param isRepl        True if run in REPL mode
 * @param includePaths  List of search paths for modules
 * @return List of compiled function and/or top-level expressions (if in REPL mode)
 */
CompilationResult compile(
  std::unique_ptr<codegen::GlobalContext>& globalContext,
  FileId                                   file,
  bool                                     isRepl = false,
  const std::vector<std::string>&          includePaths = {}
);

/**
 * 'Driver' function - will tokenize, parse, lower AST & compile the source code
 * into an object file
 *
 * @param globalContext Global context
 * @param file          ID of loaded file
 * @param filename      Name of output object file
 * @param includePaths  List of search paths for modules
*/
void compile_to_object(
  std::unique_ptr<codegen::GlobalContext>& globalContext,
  FileId                                   file,
  const std::string&                       filename,
  const std::vector<std::string>&          includePaths = {}
);

/**
 * 'Driver' function - will tokenize, parse, lower AST, compile the source code and run main() function, or if in REPL
 * mode - top-level expression
 *
 * @param globalContext Global context
 * @param file          ID of loaded file
 * @param isRepl        True if run in REPL mode
 * @param includePaths  List of search paths for modules
 */
void run(
  std::unique_ptr<codegen::GlobalContext>& globalContext,
  FileId                                   file,
  bool                                     isRepl = false,
  const std::vector<std::string>&          includePaths = {}
);

}

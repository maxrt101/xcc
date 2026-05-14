# XCC - Programming Language

[![Build XCC](https://github.com/maxrt101/xcc/actions/workflows/build.yml/badge.svg)](https://github.com/maxrt101/xcc/actions/workflows/build.yml)

A strongly-typed compiled programming language with Rust/C inspired syntax.  
Uses LLVM as a backend because of its vast support of platforms, JIT, and other features.  
XCC is a working name, it may be changed later.  
CC - is taken from GCC (GNU Compiler Collection), while XCC is not a compiler collection,  
it most certainly is a compiler. X - is just a cool letter that I like :)  

### How to run  
#### Prerequisites:  
 - GCC/clang  
 - CMake  
 - LLVM installed (and findable through CMake)

#### Build:  
 - `cmake -B build -S .`  
 - `cmake --build build -j $(nproc)`  

#### Run:  
 - `./build/xcc` - for a REPL (JIT powered interpreter)  
 - `./build/xcc -r FILE` - to run a file  
 - `./build/xcc -c FILE -o OUT` - to compile a file  
 - `XCC_LD=/path/to/ld ./build/xcc FILE1 FILE2 FILEN -o OUT` - to link or build files into an executable  

#### Arguments:
```
Usage: xcc [-h] [-v] [--verbose] [-c] [-r] [-l LIB] [-L PATH] [-I PATH] [-t TARGET] [-m MACHINE] [-o OUT_FILE] IN_FILE...
Arguments:
  -h, --help              - Print this message
  -v, --version           - Print version
  -c, --compile           - Compile into object file
  -r, --run               - Run file using JIT
  -l, --lib LIB           - Link LIB
  -L, --lib-path LIB_PATH - Add library search path
  -I, --mod-path MOD_PATH - Add module search path
  -t, --target TARGET     - Specify target triple (use 'list' to see all)
  -m, --machine MACHINE   - Specify target machine (cpu) (use 'list' to see all)
  -o, --output OUT_FILE   - Set output file name
  --log LOG_MODULE_NAME   - Enable logger for module ('*' to enable for all)
  IN_FILE...              - Input (source/object) files
Environment:
  XCC_LD                  - Path to linker executable
  XCC_LDFLAGS             - Flags to pass directly to linker
```

### Syntax  
Here's a hello world program:  
```
use stdc;

fn main() -> i32 {
  stdc::io::printf("Hello, World!\n");
  return 0;
}
```

### Features  
 - [X] Functions (user-defined, extern, forward-declarations)  
 - [X] Variables (local & global)  
 - [X] Number literals (in 8, 10, 16 bases + float point)  
 - [X] String literals (ascii only, null-terminator automatically appended + escape sequences)  
 - [X] Character literals (ascii only)
 - [X] Basic data types (`i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `void`)  
 - [X] Arithmetic operations (`+`, `-`, `*`, `/`)  
 - [X] Comparison operations (`==`, `!=`, `<`, `<=`, `>`, `>=`)  
 - [X] Pointers (dereferencing `*`, taking address of a variable `&`)  
 - [X] Subscripting (`[]`, no array type, so only usable on pointers)  
 - [X] Variadic functions (only declarations, no API to actually use it by the user)  
 - [X] Strings (null-terminated, as `i8*`)  
 - [X] String interning  
 - [X] Conditional execution (`if` statement, works just like in C)  
 - [X] Loops (only `for` is supported (syntax like in C), `while` is in the works)  
 - [X] Type casts (to some extent, represented by `as` expression)  
 - [X] User-defined types (`struct` & member access operator `.` + pointer member access `->`)  
 - [X] JIT (which allows for REPL to exist)  
 - [X] Runtime function resolution in the scope of running process using extern  
 - [X] Compiling into object files
 - [X] Scoped file modules (`use`, `use mod`, `::`)
 - [X] Nested modules (`mod name { ... }`)
 - [X] Attributes (`[]`)
 - [X] Function aliases (`[alias(...)]`)
 - [X] Environment variable resolution at compile-time
 - [X] Function pointers (`fn() -> void`)
 - [X] Target/Machine selection via command line (which enables cross-compiling)  
 - [X] Macros  
 - [ ] Procedural macros  
 - [X] Better error handling  
 - [ ] Port of libc (stdc)  
 - [ ] Standard library  
 - [X] Built-ins (sizeof, offsetof, typeof)  
 - [ ] Procedural attributes  
 - [X] Conditional imports  
 - [X] Compound struct initialization  
 - [X] Array initialization  
 - [X] Global importable constants  
 - [ ] Tuples  
 - [ ] Enums (int & tagged)  
 - [ ] Destructors (or something like `defer`)  
 - [ ] Lambdas (closures)
 - [ ] The rest of gcc/clang attributes (alias, section & packed are done)  
 - [X] Full variadic support  
 - [ ] Stable ABI/FFI  
 - [ ] String interpolation  
 - [ ] Better type inference  
 - [ ] Generics?  
 - [ ] Multithreading  
 - [ ] Inheritance?  
 - [ ] Dynamic dispatch?  
 - [ ] Build system?  
 - [ ] Dependency management system?  

### REPL  
When running XCC executable without argument - you will be dropped into the REPL.  
REPL is a Read Eval Print Loop. You can type in statements and they will be executed.  
REPL has some special commands, such as `/help`, `/quit` & `list`.  
`/help` or `/h` - shows help message.  
`/quit` or `/q` - exists the REPL.  
`/list` or `/l` - lists declared global functions.  
In REPL compiler behaves a bit differently, for example `;` is not required at the end  
of the statement, otherwise everything else should work normally.  

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
  IN_FILE...              - Input (source/object) files
Environment:
  XCC_LD                  - Path to linker executable
  XCC_LDFLAGS             - Flags to pass directly to linker
```

### Syntax  
Here's a hello world program:  
```
use stdc;

fn main(): i32 {
  stdc::io::printf("Hello, World!\n");
  return 0;
}
```

### Features  
 - Functions (user-defined, extern, forward-declarations)  
 - Variables (local & global)  
 - Number literals (in 8, 10, 16 bases + float point)  
 - String literals (ascii only, null-terminator automatically appended + escape sequences)  
 - Character literals (ascii only)
 - Basic data types (`i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `void`)  
 - Arithmetic operations (`+`, `-`, `*`, `/`)  
 - Comparison operations (`==`, `!=`, `<`, `<=`, `>`, `>=`)  
 - Pointers (dereferencing `*`, taking address of a variable `&`)  
 - Subscripting (`[]`, no array type, so only usable on pointers)  
 - Variadic functions (only declarations, no API to actually use it by the user)  
 - Strings (null-terminated, as `i8*`)  
 - String interning  
 - Conditional execution (`if` statement, works just like in C)  
 - Loops (only `for` is supported (syntax like in C), `while` is in the works)  
 - Type casts (to some extent, represented by `as` expression)  
 - User-defined types (`struct` & member access operator `.` + pointer member access `->`)  
 - JIT (which allows for REPL to exist)  
 - Runtime function resolution in the scope of running process using extern  
 - Compiling into object files
 - Scoped file modules (`use`, `use mod`, `::`)
 - Nested modules (`mod name { ... }`)
 - Attributes (`[]`)
 - Function aliases (`[alias(...)]`)
 - Environment variable resolution at compile-time
 - Function pointers (`fn() -> void`)

### REPL  
When running XCC executable without argument - you will be dropped into the REPL.  
REPL is a Read Eval Print Loop. You can type in statements and they will be executed.  
REPL has some special commands, such as `/help`, `/quit` & `list`.  
`/help` or `/h` - shows help message.  
`/quit` or `/q` - exists the REPL.  
`/list` or `/l` - lists declared global functions.  
In REPL compiler behaves a bit differently, for example `;` is not required at the end  
of the statement, otherwise everything else should work normally.  

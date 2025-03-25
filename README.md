# XCC - Programming Language

A strongly-typed compiled programming language with Rust/C inspired syntax.  
Uses LLVM as a backend because of its vast support of platforms, JIT, and other features.  
XCC is a working name, it may be changed later.  
CC - is taken from GCC (GNU Compiler Collection), while XCC is not a compiler collection,  
it most certainly is a compiler. X - is just a cool letter that I like :)  

### How to run  
Prerequisites:  
 - GCC/clang  
 - CMake  
 - LLVM installed (and findable through CMake)

Build:  
 - `cmake -B build -S .`  
 - `cd build`  
 - `make` (if makefile generator is used, you can use ninja, or anything else that CMake supports)  

Run:  
 - `./build/xcc` - for a REPL (JIT powered interpreter)  
 - `./build/xcc FILE` - to run a file  

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
 - User-defined types (`struct`)  
 - JIT (which allows for REPL to exist)  
 - Runtime function resolution in the scope of running process using extern  

### Syntax  
Here's a hello world program:  
```
extern fn printf(fmt: i8*, ...): i32;

fn main(): i32 {
  printf("Hello, World!\n");
  return 0;
}
```

Here's a more complex program showing a bit more features:  
```
extern fn printf(fmt: i8*, ...): i32;
extern fn malloc(size: u32): u8*;
extern fn free(ptr: u8*): void;

struct test_type {
  int_val: i32;
  ptr_val: i8*;
}

fn memcopy(dest: i8*, src: i8*, size: u32): u32 {
  for (var i: u32 = 0; i < size; i = i + 1) {
    dest[i] = src[i];
  }
  return 0;
}

fn main(): i32 {
  var s: i8* = "Hello, World!";
  var t: test_type;

  var tmp: i8* = malloc(14);
  t.ptr_val = tmp;

  memcopy(t.ptr_val, s, 16);

  printf("%s\n", t.ptr_val);

  free(t.ptr_val);

  return 0;
}
```

### REPL  
When running XCC executable without argument - you will be dropped into the REPL.  
REPL is a Read Eval Print Loop. You can type in statements and they will be executed.  
REPL has some special commands, such as `/help`, `/quit` & `list`.  
`/help` or `/h` - shows help message.  
`/quit` or `/q` - exists the REPL.
`/list` or `/l` - lists declared global functions.
In REPL compiler behaves a bit differently, for example `;` is not required at the end  
of the statement, otherwise everything else should work normally.  

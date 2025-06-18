# XCC - Programming Language

[![Build XCC](https://github.com/maxrt101/xcc/actions/workflows/build.yml/badge.svg)](https://github.com/maxrt101/xcc/actions/workflows/build.yml)

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
 - User-defined types (`struct` & member access operator `.` + pointer member access `->`)  
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
extern fn strlen(ptr: u8*): u32;

struct buffer_t {
  data: i8*;
  size: u32;
}

struct context_t {
  status: u32;
  buf: buffer_t;
}

fn memcopy(dest: i8*, src: i8*, size: u32): u32 {
  for (var i: u32 = 0; i < size; i = i + 1) {
    dest[i] = src[i];
  }
  return 0;
}

fn context_init(ctx: context_t*, str: i8*): void {
  ctx->buf.size = strlen(str) + 1;
  ctx->buf.data = malloc(ctx->buf.size);

  memcopy(ctx->buf.data, str, ctx->buf.size);

  ctx->status = 1;
}

fn context_deinit(ctx: context_t *): void {
  if (ctx->buf.data != 0) {
    free(ctx->buf.data);
  }
}

fn main(): i32 {
  var s: context_t;

  s.status = 0;

  context_init(&s, "Hello, World!");

  if (s.status == 1) {
    printf("%d %p '%s'\n", s.buf.size, s.buf.data, s.buf.data);
  } else {
    printf("Context is in an invalid state!\n");
  }

  context_deinit(&s);

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

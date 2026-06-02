# Core library
mod core;

#
# Bring true & false constants into the global scope
#
[prelude]
mod boolean {
  const true:  bool = 1;
  const false: bool = 0;
}

# TODO: Add const NULL value

#
# Bring panic! into scope
#
[prelude]
use mod panic;

# Contains basic memory operations (copy/fill/etc.)
use mod mem;

# Contains default global allocator
use mod alloc;

# Closures (lambdas) utilities
use mod closure;

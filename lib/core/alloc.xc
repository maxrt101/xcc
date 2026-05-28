# Implements global allocator interface
# alloc/realloc/free are marked as weak, so that they can be redefined,
# whenever a custom allocator needs to be plugged globally
# This can also be used for profiling/memory leaks detection
#
# Example:
# ```
# fn core_alloc_Allocator_alloc(size: usize) -> void* {
#   var ptr = stdc::malloc(size);
#   println!("Allocated {} bytes at {%p}", size, ptr);
#   ptr
# }
# ```
#
mod core::alloc;

use mem;
use stdc;

struct Allocator {
  [weak]
  fn alloc(size: usize) -> void* {
    stdc::malloc(size)
  }

  [weak]
  fn realloc(ptr: void*, size: usize) -> void* {
    stdc::realloc(ptr, size)
  }

  [weak]
  fn free(ptr: void*) {
    stdc::free(ptr)
  }
}

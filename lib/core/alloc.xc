# Implements global allocator interface
# alloc and free are marked as weak, so that they can be redefined,
# whenever a custom allocator needs to be plugged globally
# This can also be used for profiling/memory leaks detection
#
# Example:
# ```
# fn core_alloc_Allocator_alloc(size: usize) -> void* {
#   println!("Allocating {} bytes", size);
#   stdc::malloc(size)
# }
# ```
#
mod core::alloc;

use stdc;

struct Allocator {
  [weak]
  fn alloc(size: usize) -> void* {
    stdc::malloc(size)
  }

  [weak]
  fn free(ptr: void*) {
    stdc::free(ptr);
  }
}

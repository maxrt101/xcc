#
# Closure utilities
#

mod core::closure;

#
# Pin a closure for a lambda
#
# Will extract a closure pointer, allocate enough space
# for closure data in the heap, copy closure data there
# and update closure pointer
#
# Useful when a lambda must outlive a context in which
# it was declared. However it must be used with caution
# as any local variables captured by reference will
# become invalid, once the defining function has returned
#
macro pin(__closure) {
  var size = sizeof!(closure_type!(__closure));
  var ptr = core::alloc::Allocator::alloc(size);

  core::mem::copy(ptr, __closure[1], size);

  __closure[1] = ptr;

  __closure
}

#
# Unpin a closure for a lambda
#
# Deallocates closure buffer created by pin!
# It is undefined behaviour to call this on a
# lambda, which was not pinned
# After calling this, the closure (and by
# extension the whole lambda) becomes invalid
#
macro unpin(__closure) {
  core::alloc::Allocator::free(__closure[1]);
  __closure[1] = 0x0;
}

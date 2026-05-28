#
# Generic vector (array) container implementation
#
# This library is intentionally made as simple as possible.
# There is virtually no error handling/reporting because
# at the time of writing, the language has no panics/error
# codes/expections etc. So error handling will be added
# later, when general direction of error handling is decided.
# Also move/copy semantics are not considered as this is a
# not yet finalized part of the compiler.
#

mod std::vec;

use core;

#
# Generic sequential data container
#
# T - Value Type
# A - Allocator (`core::alloc::Allocator` by default)
#
struct Vector<T, A = core::alloc::Allocator> {
  data: T*;    # Buffer for data
  size: usize; # Size
  cap:  usize; # Capacity of `data` (allocated size)

  fn new() -> Vector {
    Vector { data: 0x0, size: 0, cap: 0 }
  }

  fn from(arr: T*, size: usize) -> Vector {
    var data = A::alloc(size) as T*;

    core::mem::copy(data, arr, size);

    Vector { data, size, cap: size }
  }

  fn with_cap(cap: usize) -> Vector {
    var data = A::alloc(size) as T*;

    Vector { data, size: 0, cap }
  }

  fn fill(size: usize, val: T) -> Vector {
    var data = A::alloc(size) as T*;

    for (var i = 0; i < size; i += 1) {
      data[i] = val;
    }

    Vector { data, size, cap: size }
  }

  [drop]
  fn drop(self) {
    if (self->data != 0x0) {
      A::free(self->data);

      self->data = 0x0;
      self->size = 0;
      self->cap  = 0;
    }
  }

  fn clone(self) -> String {
    var new_data = A::alloc(self->size + 1);
    core::mem::copy(new_data, self->data, self->size + 1);

    Vector { data: new_data, size: self->size, cap: self->size }
  }

  #
  # Clears the vector. Deallocates underlying buffer
  #
  fn clear(self) {
    self->drop();
  }

  fn length(self) -> usize {
    self->size
  }

  fn buf(self) -> T* {
    self->data
  }

  fn get(self, idx: usize) -> T {
    self->data[idx]
  }

  fn set(self, idx: usize, val: T) {
    self->data[idx] = val;
  }

  fn append(self, val: T) {
    var new_size = self->size + 1;

    if (self->cap < new_size) {
      self->data = A::realloc(self->data, new_size + 1);
    }

    self->data[self->size] = val;
    self->data[self->size+1] = 0;
    self->size = new_size;
  }

  fn pop(self) -> T {
    var last = self->data[self->size - 1];

    self->data[self->size - 1] = 0;

    self->size -= 1;

    last
  }

  fn insert(self, idx: usize, val: T) {
    if (idx > self->size) {
      return;
    }

    if (self->cap < self->size + 1) {
      self->data = A::realloc(self->data, self->size + 2);
    }

    for (var i = self->size + 1; i > idx; i -= 1) {
      self->data[i] = self->data[i - 1];
    }

    self->data[idx] = val;
    self->size += 1;
    self->data[self->size] = 0;
  }

  fn remove(self, idx: usize, size: usize) {
    if (idx + size > self->size) {
      return;
    }

    for (var i = idx; i < self->size - size; i += 1) {
      self->data[i] = self->data[i + size];
    }

    self->size -= size;
    self->data[self->size] = 0;
  }

}

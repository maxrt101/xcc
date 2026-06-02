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

  #
  # Create empty vector
  #
  # @returns Newly created vector
  #
  fn new() -> Vector {
    Vector { data: 0x0, size: 0, cap: 0 }
  }

  #
  # Create vector from array data
  #
  # @param arr  Pointer to data, with which to initialize vector
  # @param size Size of `arr` (in elements)
  # @returns Newly created vector
  #
  fn from(arr: T*, size: usize) -> Vector {
    var data = A::alloc(size * sizeof!(T)) as T*;

    core::mem::copy(data, arr, size * sizeof!(T));

    Vector { data, size, cap: size }
  }

  #
  # Create a vector with pre-allocated buffer
  #
  # @param cap Requested capacity of vector
  # @returns Newly created vector
  #
  fn with_cap(cap: usize) -> Vector {
    var data = A::alloc(cap * sizeof!(T)) as T*;

    Vector { data, size: 0, cap }
  }

  #
  # Create a vector, filling it with specified value
  #
  # @param size Size of new vector
  # @param val  Value to fill it with
  # @returns Newly created vector
  #
  fn fill(size: usize, val: T) -> Vector {
    var data = A::alloc(size * sizeof!(T)) as T*;

    for (var i = 0; i < size; i += 1) {
      data[i] = val;
    }

    Vector { data, size, cap: size }
  }

  #
  # Destructor for Vector
  #
  [drop]
  fn drop(self) {
    if (self->data != 0x0) {
      A::free(self->data);

      self->data = 0x0;
      self->size = 0;
      self->cap  = 0;
    }
  }

  #
  # Create a new vector from current, copying all of the data
  #
  fn clone(self) -> Vector {
    var new_data = A::alloc(self->size * sizeof!(T));
    core::mem::copy(new_data, self->data, self->size);

    Vector { data: new_data, size: self->size, cap: self->size }
  }

  #
  # Clears the vector. Deallocates underlying buffer
  #
  fn clear(self) {
    self->drop();
  }

  #
  # Return size of vector
  #
  # @returns Size of vector
  #
  fn length(self) -> usize {
    self->size
  }

  #
  # Return pointer to underlying buffer
  #
  # @returns Pointer to underlying buffer
  #
  fn buf(self) -> T* {
    self->data
  }

  #
  # Get element at index
  #
  # @param idx Index of the element
  # @returns Element at `idx`
  #
  fn get(self, idx: usize) -> T {
    self->data[idx]
  }

  #
  # Set element at index
  #
  # @param idx Index of the element
  # @param val Value to set
  #
  fn set(self, idx: usize, val: T) {
    self->data[idx] = val;
  }

  #
  # Append a value to the end of vector
  #
  # @param val Value to append
  #
  fn append(self, val: T) {
    var new_size = self->size + 1;

    if (self->cap < new_size) {
      self->data = A::realloc(self->data, new_size * sizeof!(T));
    }

    self->data[self->size] = val;
    self->data[self->size+1] = 0;
    self->size = new_size;
  }

  #
  # Remove a value to the end of vector
  #
  # @returns Removed value
  #
  fn pop(self) -> T {
    var last = self->data[self->size - 1];

    self->data[self->size - 1] = 0;

    self->size -= 1;

    last
  }

  #
  # Insert a value at a specified index
  #
  # @param idx Index to insert `val` at
  # @param val Value to insert
  #
  fn insert(self, idx: usize, val: T) {
    if (idx > self->size) {
      return;
    }

    var new_size = self->size + 1;

    if (self->cap < new_size) {
      self->data = A::realloc(self->data, new_size * sizeof!(T));
    }

    for (var i = new_size; i > idx; i -= 1) {
      self->data[i] = self->data[i - 1];
    }

    self->data[idx] = val;
    self->size += 1;
    self->data[self->size] = 0;
  }

  #
  # Erase element at `idx`
  #
  # @param idx Index to erase element at
  #
  fn erase(self, idx: usize) {
    self->remove(idx, 1);
  }

  #
  # Remove a range of elements
  #
  # @param idx  Index for the start of range
  # @param size Size of range
  #
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

  #
  # Append all elements from `rhs` to current vector
  #
  # @param rhs Vector to merge into current
  #
  fn extend(self, rhs: Vector*) {
    # TODO: Use core::mem::copy
    for (var i = 0; i < rhs->size; i+= 1) {
      self->append(rhs->get(i));
    }
  }

  #
  # Find element by value
  #
  # @param val Value to find
  # @returns index of found value, or -1 if not present
  #
  fn find(self, val: T) -> isize {
    for (var i = 0; i < self->size; i += 1) {
      if (self->data[i] == val) {
        return i;
      }
    }

    return -1;
  }

  #
  # Find element by predicate
  #
  # @param pred Predicate accepting a pointer to a value, and returning `true` if value is suitable
  # @returns index of found value, or -1 if not present
  #
  fn find_if(self, pred: fn (T*) -> bool) -> isize {
    for (var i = 0; i < self->size; i += 1) {
      if (pred(&self->data[i])) {
        return i;
      }
    }

    return -1;
  }

  # TODO: capacity
  # TODO: grow
  # TODO: contains
  # TODO: contains_if
  # TODO: count
  # TODO: count_if
  # TODO: get_all?
  # TODO: get_all_if?
}

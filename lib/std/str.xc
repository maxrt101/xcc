#
# Basic string utilities and String class
#
# String is a wrapper for a C-String with an ability to
# dynamically grow the underlying buffer, when needed.
#
# Module `raw` is for working with raw c-strings.
#
# This library is intentionally made as simple as possible.
# There is virtually no error handling/reporting because
# at the time of writing, the language has no panics/error
# codes/expections etc. So error handling will be added
# later, when general direction of error handling is decided.
#

mod std::str;

use core;
use stdc;

#
# Module for raw string manipulation
# Raw string being a C-style char pointer, terminated with '\0'
#
mod raw {
  #
  # Calculate raw string size
  #
  # @param str Pointer to c-string
  # @returns size of `str`, without the null-terminator
  #
  fn size(str: i8*) -> usize {
    stdc::strlen(str)
  }

  #
  # Compare 2 strings, returning `true` if they are equal
  #
  # @param lhs Pointer to first string
  # @param rhs Pointer to second string
  # @returns `true` if `lhs` & `rhs` are equal, `false` otherwise
  #
  fn equals(lhs: i8*, rhs: i8*) -> bool {
    var lhs_size = stdc::strlen(lhs);
    var rhs_size = stdc::strlen(rhs);

    if (lhs_size != rhs_size) {
      return false;
    }

    core::mem::compare(lhs, rhs, lhs_size)
  }

  #
  # Returns `true` if `lhs` starts with `rhs`
  #
  # @param lhs String to check for `rhs`
  # @param rhs Substring to check for
  # @returns `true` if lhs starts with rhs
  #
  fn starts_with(lhs: i8*, rhs: i8*) -> bool {
    var lhs_size = stdc::strlen(rhs);
    var rhs_size = stdc::strlen(rhs);

    if (lhs_size < rhs_size) {
      return false;
    }

    core::mem::compare(lhs, rhs, rhs_size)
  }

  #
  # Returns `true` if `lhs` ends with `rhs`
  #
  # @param lhs String to check for `rhs`
  # @param rhs Substring to check for
  # @returns `true` if lhs ends with rhs
  #
  fn ends_with(lhs: i8*, rhs: i8*) -> bool {
    var lhs_size = stdc::strlen(rhs);
    var rhs_size = stdc::strlen(rhs);

    if (lhs_size < rhs_size) {
      return false;
    }

    core::mem::compare(lhs + lhs_size - rhs_size, rhs, rhs_size)
  }

  #
  # Return `true` if current string contains substring
  #
  # @param lhs String to check for substring in
  # @param rhs Substring to check for
  # @returns index into `lhs`, at which `rhs` starts, if found, -1 otherwise
  #
  fn find(lhs: i8*, rhs: i8*) -> isize {
    var lhs_size = stdc::strlen(lhs);
    var rhs_size = stdc::strlen(rhs);

    if (lhs_size < rhs_size) {
      return -1;
    }

    for (var i = 0; i < lhs_size; i += 1) {
      if (lhs_size - i < rhs_size) {
        return -1;
      }

      if (core::mem::compare(lhs + i, rhs, rhs_size)) {
        return i;
      }
    }

    return -1;
  }

  #
  # Return `true` if current string contains substring
  #
  # @param lhs String to check for substring in
  # @param rhs Substring to check for
  # @returns `true` if `lhs` contains `rhs`, `false` otherwise
  #
  fn contains(lhs: i8*, rhs: i8*) -> bool {
    if (find(lhs, rhs) != -1) {
      return true;
    }

    false
  }
}

# TODO: String Flags (STRING_FLAG_INITIALIZED, STRING_FLAG_STATIC)
# TODO: Static strings (disallowed realloc, no free in drop())

#
# Basic implementation of a string
#
# T - Base Char Type (`i8` by default)
# A - Allocator (`core::alloc::Allocator` by default)
#
# Base Char Type is intentionally generic, to maybe
# facilitate non-ascii strings in the future
#
struct String<T = i8, A = core::alloc::Allocator> {
  data: T*;    # Raw string data (null-terminated)
  size: usize; # Size (up to, but not including, null terminator)
  cap:  usize; # Capacity of `data` (allocated size)

  #
  # Create new empty string instance
  #
  fn new() -> String {
    String { data: 0x0, size: 0, cap: 0 }
  }

  #
  # Initialize string from raw string data
  #
  # @param str Raw C-String
  # @returns Newly created String instance
  #
  fn from(str: T*) -> String {
    var size = stdc::strlen(str);
    var data = A::alloc(size + 1) as T*;

    stdc::memcpy(data, str, size + 1);

    String { data, size, cap: size }
  }

  #
  # Create an empty string, with pre-allocated buffer of specified size
  #
  # @param cap Desired capacity
  # @returns Newly created String instance, with a buffer of size `cap`
  #
  fn with_cap(cap: usize) -> String {
    var data = A::alloc(cap + 1) as T*;

    String { data, size: 0, cap }
  }

  #
  # Create a string of specified size, filling it with `ch`
  #
  # @param size Size of the string
  # @param ch   Character to fill the string with
  #
  fn fill(size: usize, ch: T) -> String {
    var data = A::alloc(size + 1) as T*;

    # TODO: If T != i8 - will not work properly
    core::mem::fill(data, ch as i8, size);

    data[size] = 0;

    String { data, size, cap: size }
  }

  #
  # Destructor for String
  #
  # Deallocates the underlying buffer, if it
  # was allocated, and resets all fields
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
  # Clones current string
  #
  # @returns A copy of original string
  #
  fn clone(self) -> String {
    var new_data = A::alloc(self->size + 1);
    core::mem::copy(new_data, self->data, self->size + 1);

    String { data: new_data, size: self->size, cap: self->size }
  }

  #
  # Clears the string. Deallocates underlying buffer
  #
  fn clear(self) {
    self->drop();
  }

  #
  # Retrieve string length
  #
  # @returns String length, without the null-terminator
  #
  fn length(self) -> usize {
    self->size
  }

  #
  # Return pointer to null-terminated data
  #
  # @returns A pointer to a null-terminated c-string
  #
  fn c_str(self) -> T* {
    self->data
  }

  #
  # Get character at index
  #
  # @param idx Index of desired char
  # @returns Character at index
  #
  fn get(self, idx: usize) -> T {
    self->data[idx]
  }

  #
  # Set character at index
  #
  # @param idx Index to set the character in
  # @param val The character
  #
  fn set(self, idx: usize, val: T) {
    self->data[idx] = val;
  }

  #
  # Append character at the end
  #
  # @param ch Charter to append
  #
  fn push(self, ch: T) {
    var new_size = self->size + 1;

    if (self->cap < new_size) {
      self->data = A::realloc(self->data, new_size + 1);
    }

    self->data[self->size] = ch;
    self->data[self->size+1] = 0;
    self->size = new_size;
  }

  #
  # Remove character from the end
  #
  # @returns Removed character
  #
  fn pop(self) -> T {
    var last = self->data[self->size - 1];

    self->data[self->size - 1] = 0;

    self->size -= 1;

    last
  }

  #
  # Insert a character at a specified index
  #
  # @param idx Index to put the `val` into
  # @param val The character
  #
  fn put(self, idx: usize, val: T) {
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

  #
  # Insert a string at a specified index
  #
  # @param idx Index to insert the string at
  # @param str String to insert
  #
  fn insert(self, idx: usize, str: String*) {
    if (idx > self->size) {
      return;
    }

    var combined_size = self->size + str->size;

    if (self->cap < combined_size + 1) {
      self->data = A::realloc(self->data, combined_size + 1);
    }

    for (var i = 1; i < str->size + 1; i += 1) {
      self->data[combined_size - i] = self->data[self->size - i];
    }

    core::mem::copy(self->data + idx, str->data, str->size);

    self->size = combined_size;
    self->data[self->size] = 0;
  }

  #
  # Insert a raw string at a specified index
  #
  # @param idx Index to insert the string at
  # @param str String to insert
  #
  fn insert_raw(self, idx: usize, str: T*) {
    if (idx > self->size) {
      return;
    }

    var str_size = stdc::strlen(str);
    var combined_size = self->size + str_size;

    if (self->cap < combined_size + 1) {
      self->data = A::realloc(self->data, combined_size + 1);
    }

    for (var i = 1; i < str_size + 1; i += 1) {
      self->data[combined_size - i] = self->data[self->size - i];
    }

    core::mem::copy(self->data + idx, str, str_size);

    self->size = combined_size;
    self->data[self->size] = 0;
  }

  #
  # Remove a substring from string
  #
  # @param idx  Index that corresponds to the start of a substring that has to be removed
  # @param size Size of the substring
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
  # Append (concatenate) another string to current
  #
  # @param rhs String to append
  #
  fn append(self, rhs: String*) {
    var combined_size = self->size + rhs->size;

    if (self->cap < combined_size + 1) {
      self->data = A::realloc(self->data, combined_size + 1);
      self->cap = combined_size;
    }

    core::mem::copy(self->data + self->size, rhs->data, rhs->size);

    self->size = combined_size;
    self->data[combined_size] = 0;
  }

  #
  # Append (concatenate) raw string data to current
  #
  # @param rhs C-String to append
  #
  fn append_raw(self, rhs: T*) {
    var rhs_size = stdc::strlen(rhs);
    var combined_size = self->size + rhs_size;

    if (self->cap < combined_size + 1) {
      self->data = A::realloc(self->data, combined_size + 1);
      self->cap = combined_size;
    }

    core::mem::copy(self->data + self->size, rhs, rhs_size);

    self->size = combined_size;
    self->data[combined_size] = 0;
  }

  #
  # Compare with another string
  #
  # @param rhs String to compare with
  # @returns `true` if strings are equal
  #
  fn equals(self, rhs: String*) -> bool {
    if (self->size != rhs->size) {
      return false;
    }

    core::mem::compare(self->data, rhs->data, self->size)
  }

  #
  # Compare with raw string data
  #
  # @param rhs String to compare with
  # @returns `true` if strings are equal
  #
  fn equals_raw(self, rhs: T*) -> bool {
    var rhs_size = stdc::strlen(rhs);

    if (self->size != rhs_size) {
      return false;
    }

    core::mem::compare(self->data, rhs, self->size)
  }

  #
  # Check is strings starts with sequence
  #
  # @param rhs Substring to check for
  # @returns `true` if starts with rhs
  #
  fn starts_with(self, rhs: String*) -> bool {
    if (self->size < rhs->size) {
      return false;
    }

    core::mem::compare(self->data, rhs->data, rhs->size)
  }

  #
  # Check is strings starts with raw sequence
  #
  # @param rhs Substring to check for
  # @returns `true` if starts with rhs
  #
  fn starts_with_raw(self, rhs: T*) -> bool {
    var rhs_size = stdc::strlen(rhs);

    if (self->size < rhs_size) {
      return false;
    }

    core::mem::compare(self->data, rhs, rhs_size)
  }

  #
  # Check is strings ends with sequence
  #
  # @param rhs Substring to check for
  # @returns `true` if ends with rhs
  #
  fn ends_with(self, rhs: String*) -> bool {
    if (self->size < rhs->size) {
      return false;
    }

    core::mem::compare(self->data + self->size - rhs->size, rhs->data, rhs->size)
  }

  #
  # Check is strings ends with raw sequence
  #
  # @param rhs Substring to check for
  # @returns `true` if ends with rhs
  #
  fn ends_with_raw(self, rhs: T*) -> bool {
    var rhs_size = stdc::strlen(rhs);

    if (self->size < rhs_size) {
      return false;
    }

    core::mem::compare(self->data + self->size - rhs_size, rhs, rhs_size)
  }

  #
  # Find a substring in current string
  #
  # @param rhs String to search for
  # @returns index of first occurrence, if found and -1 otherwise
  #
  fn find(self, rhs: String*) -> isize {
    if (self->size < rhs->size) {
      return -1;
    }

    for (var i = 0; i < self->size; i += 1) {
      if (self->size - i < rhs->size) {
        return -1;
      }

      if (core::mem::compare(self->data + i, rhs->data, rhs->size)) {
        return i;
      }
    }

    return -1;
  }

  #
  # Find a raw substring in current string
  #
  # @param rhs String to search for
  # @returns index of first occurrence, if found and -1 otherwise
  #
  fn find_raw(self, rhs: T*) -> isize {
    var rhs_size = stdc::strlen(rhs);

    if (self->size < rhs_size) {
      return -1;
    }

    for (var i = 0; i < self->size; i += 1) {
      if (self->size - i < rhs_size) {
        return -1;
      }

      if (core::mem::compare(self->data + i, rhs, rhs_size)) {
        return i;
      }
    }

    return -1;
  }

  #
  # Return `true` if current string contains substring
  #
  # @param rhs Substring to check for
  # @returns `true` if contains `rhs`, `false` otherwise
  #
  fn contains(self, rhs: String*) -> bool {
    if (self->find(rhs) != -1) {
      return true;
    }

    false
  }

  #
  # Return `true` if current string contains raw substring
  #
  # @param rhs Substring to check for
  # @returns `true` if contains `rhs`, `false` otherwise
  #
  fn contains_raw(self, rhs: T*) -> bool {
    if (self->find_raw(rhs) != -1) {
      return true;
    }

    false
  }

  #
  # Allocate a new string, into which a specified part of original string will be placed
  # start & end can be negative, in which case -1 points to the last char in the string
  # (not counting the null terminator)
  #
  # @param start Start index of the substring
  # @param end   End index of the substring
  # @returns New string, which is self[start; end]
  #
  fn slice(self, start: isize, end: isize) -> String {
    if (start < 0) {
      start = self->size + start + 1;
    }

    if (end < 0) {
      end = self->size + end + 1;
    }

    # TODO: Add checks
    # if (start > end) {}

    var size = end - start;

    var res = with_cap(size);

    core::mem::copy(res.data, self->data + start, size);

    res.size = size;

    # It's OK because every allocation for `size`, will implicitly
    # allocate `size+1` to accommodate null-terminator
    res.data[size] = 0;

    res
  }
}

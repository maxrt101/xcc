mod core::mem;

# TODO: Use llvm.memcpy
fn copy(dest: void*, src: void*, size: usize) {
  var dptr = dest as u8*;
  var sptr = src as u8*;

  for (var i = 0; i < size; i += 1) {
    dptr[i] = sptr[i];
  }
}

# TODO: Use llvm.memmove
fn move(dest: void*, src: void*, size: usize) {
  var dptr = dest as u8*;
  var sptr = src as u8*;

  for (var i = 0; i < size; i += 1) {
    dptr[i] = sptr[i];
  }
}

# TODO: Use llvm.memset
fn fill(dest: void*, value: u8, size: usize) {
  var dptr = dest as u8*;

  for (var i = 0; i < size; i += 1) {
    dptr[i] = value;
  }
}

# TODO: Use llvm.memcmp
fn compare(buf1: void*, buf2: void*, size: usize) -> bool {
  var ptr1 = buf1 as u8*;
  var ptr2 = buf2 as u8*;

  for (var i = 0; i < size; i += 1) {
    if (ptr1[i] != ptr2[i]) {
      return false;
    }
  }

  return true;
}

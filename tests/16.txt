extern fn printf(fmt: i8*, ...): i32;
extern fn malloc(size: u32): u8*;
extern fn free(ptr: u8*): void;

fn copy_buffer(dest: i8*, src: i8*, size: u32): u32 {
  for (var i: u32 = 0; i < size; i = i + 1) {
    dest[i] = src[i];
  }
  return 0;
}

fn main(): i32 {
  var s: i8* = "Hello, World!";

  var buf: i8* = malloc(14);

  copy_buffer(buf, s, 16);

  printf("original: '%s' copied: '%s'\n", s, buf);

  free(buf);

  return 0;
}

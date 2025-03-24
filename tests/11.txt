extern fn xcc_putc(c: i32): i32;
extern fn xcc_putd(c: i32): i32;
extern fn xcc_putud(c: i32): i32;
extern fn xcc_putux(c: i32): i32;

extern fn malloc(size: u32): u8*;
extern fn free(ptr: u8*): void;

fn main(): i32 {
  var mem: u8* = malloc(4);
  var p32: u32* = mem as u32*;

  xcc_putux(mem); xcc_putc(10);

  xcc_putux(p32); xcc_putc(10);

  *p32 = 128;

  xcc_putd(*p32); xcc_putc(10);

  *p32 = 256;

  xcc_putd(*p32); xcc_putc(10);

  free(p32);

  return 0;
}
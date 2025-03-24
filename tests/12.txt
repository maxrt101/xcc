extern fn xcc_putc(c: i32): i32;
extern fn xcc_putd(c: i32): i32;
extern fn xcc_putud(c: i32): i32;
extern fn xcc_putux(c: i32): i32;

extern fn malloc(size: u32): u8*;
extern fn free(ptr: u8*): void;

fn main(): i32 {
  var mem: u8* = malloc(4);

  xcc_putux(mem); xcc_putc(10);

  mem[0] = 12;
  mem[1] = 13;
  mem[2] = 14;
  mem[3] = 15;

  xcc_putd(mem[0]); xcc_putc(10);
  xcc_putd(mem[1]); xcc_putc(10);
  xcc_putd(mem[2]); xcc_putc(10);
  xcc_putd(mem[3]); xcc_putc(10);

  free(mem);

  return 0;
}
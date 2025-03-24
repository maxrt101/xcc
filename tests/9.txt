extern fn xcc_putc(c: i32): i32;
extern fn xcc_putd(c: i32): i32;
extern fn xcc_putud(c: i32): i32;
extern fn xcc_putux(c: i32): i32;

fn main(): u32 {
  var x: i32 = 128;

  var ptr: i32* = &x;

  xcc_putux(ptr as u32);
  xcc_putc(10);

  xcc_putd(*ptr);
  xcc_putc(10);

  *ptr = 25;

  xcc_putd(*ptr);
  xcc_putc(10);

  return ptr as u32;
}
extern fn xcc_putc(c: i32): i32;
extern fn xcc_putd(c: i32): i32;
extern fn xcc_putud(c: i32): i32;
extern fn xcc_putux(c: i32): i32;

fn set_i32_ptr(ptr: i32*, val: i32): i32 {
  *ptr = val;
  return 0;
}

fn main(): u32 {
  var x: i32 = 128;

  xcc_putux(&x);
  xcc_putc(10);

  xcc_putd(x);
  xcc_putc(10);

  set_i32_ptr(&x, 256);

  xcc_putd(x);
  xcc_putc(10);

  return &x as u32;
}
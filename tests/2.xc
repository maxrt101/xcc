
extern fn xcc_putc(c: i32): i32;

fn test(a: i32, b: i32): i32 {
  xcc_putc(120);
  xcc_putc(121);
  return a + b;
}

fn main(): i32 {
  return test(10, 20);
}

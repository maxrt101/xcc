
extern fn xcc_putc(c: i32): i32;

extern fn div(a: i32, b: i32): i32;
extern fn sin(a: f64): f64;

fn test(x: i32): f64 {
  var a: f64 = sin(x as f64);

  if (a > 0) {
    xcc_putc(120);
    return 1;
  } else {
    xcc_putc(121);
    return 0;
  }
}

fn main(): i32 {
  return test(100);
}

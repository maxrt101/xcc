
extern fn xcc_putc(c: i32): i32;

fn test(a: i32, b: i32): i32 {
  if (a == 20) {
    if (b == 20) {
      xcc_putc(120);
      return a;
    }
  } else {
    xcc_putc(121);
    return b;
  }
  return a + b;
}

fn main(): i32 {
  test(10, 20);
  return test(20, 20);
}

#fn test2(x: i32): i32 { if (x == 10) { xcc_putc(120); } else { xcc_putc(121); } return x; }

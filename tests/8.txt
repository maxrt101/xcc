extern fn xcc_putc(c: i32): i32;
extern fn xcc_putd(c: i32): i32;

fn test(x: i32): i32 {
  for (var i: i32 = 0; i < x; i = i + 1) {
    if (i < 10) {
      xcc_putd(i);
      xcc_putc(10);
      #return 700;
    } else {
      xcc_putd(42);
      xcc_putc(10);
      return 800;
    }
  }

  #return 42;
}

fn main(): i32 {
  return test(15);
}

extern fn xcc_putc(c: i32): i32;

fn test(x: i32): i32 {
  for (var i: i32 = 0; i < x; i = i + 1) {
    xcc_putc(i + 97);
  }

  return x;
}

fn main(): i32 {
  test(26);
  xcc_putc(10);
  return 42;
}

#fn test(x: i32): i32 { for (var i: i32 = 0; i < x; i = i + 1) { xcc_putc(i + 97); } return x; }

#fn test2(x: i32): i32 { for (var i: i32 = 0; i < x; i = i + 1) { if (i < 10) { xcc_putc(i + 97); } else { return i; } } return x; }

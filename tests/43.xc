extern fn printf(fmt: u8*, ...) -> i32;

type F = fn (i32, i32) -> i32;

struct Test {
  x: i32;
  y: i16;
  z: i32;
}

fn add32(a: i32, b: i32) -> i32 {
  return a + b;
}

fn test_arg(a: i32) {
  var x: typeof!(a) = 42;

  printf("x is i32: %d\n", is_same!(x, i32));
}

fn main() -> i32 {
  var x: i64 = 0;
  var y: typeof!(x) = 42;

  printf("y is i64: %d\n", is_same!(y, i64));

  var f: typeof!(add32);

  printf("f is F: %d\n", is_same!(f, F));
  printf("f is add32: %d\n", is_same!(f, add32));

  test_arg(0);

  return 0;
}

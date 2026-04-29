extern fn printf(fmt: u8*, ...) -> i32;

type F = fn (i32, i32) -> i32;

fn add32(a: i32, b: i32) -> i32 {
  return a + b;
}

fn test_arg(a: i32) {
  printf("a is i32: %d\n", is_same!(a, i32));
}

fn main() -> i32 {
  var a: i32 = 42;

  printf("i32=%d\n", is_same!(i32, i32));
  printf("a=%d\n", is_same!(a, i32));
  printf("add32=%d\n", is_same!(add32, F));

  test_arg(0);

  return 0;
}

extern fn printf(fmt: i8*, ...) -> i32;

fn test(x: i32) -> i32 {
  return x * x;
}

fn main() -> i32 {
  var f: fn(i32) -> i32 = test;

  printf("f=%p f(4)=%d\n", f[0], f(4));

  return 0;
}

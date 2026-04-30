extern fn printf(fmt: u8*, ...) -> i32;

fn main() -> i32 {
  printf("%s\n", str!(1 + 2 + x));
  printf("%s\n", str!(fn test(a: i32, b: i32) -> i32 { return a + b; }));
  printf("%s\n", strf!(fn test(a: i32, b: i32) -> i32 { return a + b; }));

  return 0;
}
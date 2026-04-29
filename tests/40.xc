extern fn printf(fmt: u8*, ...) -> i32;

fn main() -> i32 {
  var abc123: i32 = 42;

  printf("cat=%d\n", cat!(abc, 123));

  return 0;
}

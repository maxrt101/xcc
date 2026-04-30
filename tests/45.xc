extern fn printf(fmt: u8*, ...) -> i32;

fn main() -> i32 {
  printf("res=%d\n", add!(add!(1, 2), 4));

  return 0;
}
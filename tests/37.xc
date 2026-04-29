extern fn printf(fmt: u8*, ...) -> i32;

fn main() -> i32 {
  printf("block=%d\n", { var x: i32 = 40; x += 2; x; });

  return 0;
}

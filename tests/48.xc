extern fn printf(fmt: u8*, ...) -> i32;

fn main() -> i32 {
  repeat!(5, i, printf("%d\n", i));

  return 0;
}
extern fn printf(fmt: u8*, ...) -> i32;

fn main() -> i32 {
  printf("3=%d\n", add!(1, 2));
  printf("5=%d\n", sub!(6, 1));
  printf("2=%d\n", inc!(1));
  printf("4=%d\n", dec!(5));
  printf("42=%d\n", int!("0x2a"));
  return 0;
}

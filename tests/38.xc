extern fn printf(fmt: u8*, ...) -> i32;

macro add(a, b) {
  a + b
}

macro print_i(x) {
  printf("%d\n", x);
}

fn main() -> i32 {
  printf("add=%d\n", add!(31, 11));

  print_i!(69);

  return 0;
}

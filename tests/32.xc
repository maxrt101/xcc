extern fn printf(fmt: i8*, ...) -> i32;

fn main() -> i32 {
  printf("TEST_VAR=%s\n", $TEST_VAR);

  return 0;
}

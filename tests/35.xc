extern fn printf(fmt: u8*, ...) -> i32;

mod test {
  fn long_function_name(x: i32) {
    printf("test::long_function_name: %d\n", x);
  }
}

[alias(test::long_function_name)]
fn alias_fn(x: i32);

fn main() -> i32 {
  alias_fn(42);
  return 0;
}


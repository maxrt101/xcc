extern fn printf(fmt: u8*, ...) -> i32;

macro zero() { 0 }
macro one() { 1 }

fn main() -> i32 {
  printf("first=%s\n", cond!(one!(), "true", "false"));
  printf("second=%s\n", cond!(zero!(), "true", "false"));

  return 0;
}
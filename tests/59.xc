use stdc;

fn main() -> i32 {
  warn!("This is a warning");
  assert!(1, "This shouldn't fail");
  assert!(0, "This should fail");
  return 0;
}

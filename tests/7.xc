extern fn xcc_putd(c: i32): i32;

fn main(): i32 {
  xcc_putd(42);
  return 42;
}

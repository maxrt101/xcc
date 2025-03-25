extern fn printf(fmt: i8*, ...): i32;

fn main(): i32 {
  var num: u32 = 0b0111;
  var mask: u32 = 0b1101;

  printf("%x\n", num & mask);

  return 0;
}
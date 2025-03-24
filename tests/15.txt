extern fn printf(fmt: u8*, ...): i32;

fn main(): i32 {
  return printf("XCC Test 0x%x\n", 0xff);
}

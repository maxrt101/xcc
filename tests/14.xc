extern fn xcc_puts(s: u8*): i32;

fn main(): i32 {
  var s: i8* = "Hello, World!\n";

  xcc_puts(s);

  return 0xff;
}
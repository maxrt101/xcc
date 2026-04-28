extern fn printf(fmt: u8*, ...) -> i32;

type String = u8*;
type Ptr32 = i32*;

fn main() -> i32 {
  var s: String = "abc";

  var x: i32 = 42;
  var p: Ptr32 = &x;

  printf("s=%s\n", s);
  printf("*p=%d\n", *p);

  return 0;
}

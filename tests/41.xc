extern fn printf(fmt: u8*, ...) -> i32;

struct Test {
  x: i32;
  y: i16;
  z: i32;
}

fn main() -> i32 {
  printf("i8=%d\n", sizeof!(i8));
  printf("i16=%d\n", sizeof!(i16));
  printf("i32=%d\n", sizeof!(i32));
  printf("i64=%d\n", sizeof!(i64));
  printf("f32=%d\n", sizeof!(f32));
  printf("f64=%d\n", sizeof!(f64));
  printf("usize=%d\n", sizeof!(usize));
  printf("Test=%d\n", sizeof!(Test));

  var x: i64 = 0;
  printf("x=%d\n", sizeof!(x));

  var t: Test;
  printf("t=%d\n", sizeof!(t));

  return 0;
}

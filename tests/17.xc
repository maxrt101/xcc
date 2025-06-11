extern fn printf(fmt: i8*, ...): i32;

var x: i32 = 125;

fn main(): i32 {
  printf("%d\n", x);
  x = 12;
  printf("%d\n", x);
  return 0;
}

extern fn printf(fmt: i8*, ...): i32;

fn main(): i32 {
  var num: i64 = 100;

  num += 30;

  num /= 2;

  printf("%d\n", num);

  return 0;
}
extern fn printf(fmt: i8*, ...): i32;

var str: i8* = "Test String";

fn main(): i32 {
  printf("'%s'\n", str);
  return 0;
}

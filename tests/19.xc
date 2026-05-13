extern fn printf(fmt: i8*, ...) -> i32;
extern fn strlen(str: i8*) -> u32;

var str = i8 {0, 1};

fn main() -> i32 {
  var len: u32 = strlen(str);

  str[0] = 'X';

  printf("'%c' %d\n", str[0], str[0]);
  return 0;
}

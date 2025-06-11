extern fn printf(fmt: i8*, ...): i32;
extern fn strlen(str: i8*): u32;

var str: i8* = "Test String";

fn main(): i32 {
  var len: u32 = strlen(str);

  str[0] = 'X'; # "X"[0]

  for (var i: u32 = 0; i < len; i = i + 1) {
    printf("'%c' %d\n", str[i], str[i]);
  }

  printf("'%s'\n", str);
  return 0;
}

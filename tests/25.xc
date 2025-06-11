extern fn printf(fmt: i8*, ...): i32;

fn ret_str(): i8* {
  return "Hello, World!";
}

fn main(): i32 {
  var x = ret_str();

  printf("%s\n", x);

  return 0;
}
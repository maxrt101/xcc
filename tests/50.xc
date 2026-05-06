use stdc::printf;
use stdc::{va_list, va_start, va_end, va_arg};

fn variadic(fmt: u8*, ...) {
  var ap: va_list;
  va_start!(ap, fmt);
  printf("variadic: %s %d\n", fmt, va_arg!(ap, i64));
  va_end!(ap);
}

fn main() -> i32 {
  printf("printf in scope\n");
  variadic("variadic in scope", 42);
  return 0;
}

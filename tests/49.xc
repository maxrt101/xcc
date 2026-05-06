use stdc;

fn variadic_test(fmt: u8*, ...) {
  var ap: stdc::arg::va_list;
  stdc::arg::va_start!(ap, fmt);

  var arg = stdc::arg::va_arg!(ap, i64);
  stdc::io::printf("%s %d\n", fmt, arg);

  stdc::arg::va_end!(ap);
}

fn main() -> i32 {
  variadic_test("test", 42);
  return 0;
}


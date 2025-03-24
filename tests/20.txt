extern fn printf(fmt: i8*, ...): i32;

struct test_t {
  int_val: i32;
  ptr_var: i8*;
}

fn main(): i32 {
  var t: test_t;

  t.int_val = 100;

  printf("Test %d\n", t.int_val);
  return 0;
}

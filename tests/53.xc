use stdc::printf;

fn ret_arr() -> i32[2] {
  return [] { 32, 2 };
}

fn main() -> i32 {
  var arr: i32[2] = [] { 69, 420 };

  printf("var=%d %d\n", arr[0], arr[1]);

  var r_arr = ret_arr();

  printf("ret=%d %d\n", r_arr[0], r_arr[1]);

  return 0;
}

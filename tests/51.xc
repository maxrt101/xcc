use stdc::printf;

var arr: i8[4];

fn modify(ptr: i8*, idx: usize) {
  ptr[idx] = 151;
}

fn main() -> i32 {
  var a: i8[4];

  a[0] = 42;
  a[1] = 69;

  modify(a, 2);

  printf("local 0=%d 1=%d 2=%d\n", a[0], a[1], a[2]);

  arr[0] = 69;
  arr[1] = 42;

  modify(arr, 2);

  printf("global 0=%d 1=%d 2=%d\n", arr[0], arr[1], arr[2]);

  return 0;
}

use stdc::printf;

const N: usize = 4;

mod test {
  const N: usize = 18;

  fn getN() -> usize {
    return N;
  }
}

fn main() -> i32 {
  printf("N=%zu\n", N);

  var arr: i32[N] = [] {4, 3, 2};
  printf("arr=%d %d %d %d\n", arr[0], arr[1], arr[2], arr[3]);

  printf("size=%zu\n", sizeof!(arr));

  printf("test::N=%zu %zu\n", test::N, test::getN());

  return 0;
}

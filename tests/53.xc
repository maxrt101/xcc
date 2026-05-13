use stdc::printf;

fn ret_arr() -> i32[2] {
  return [] { 32, 2 };
}

fn main() -> i32 {
  var arr: i32[2] = [] { 69, 420 };
  printf("var=%d %d\n", arr[0], arr[1]);

  var r_arr = ret_arr();
  printf("ret=%d %d\n", r_arr[0], r_arr[1]);

  var mat: i32[3][3] = [] {
    [] {1, 2, 3},
    [] {4, 5, 6},
    [] {7, 8, 9}
  };

  printf("mat[0]={%d, %d, %d}\n", mat[0][0], mat[0][1], mat[0][2]);
  printf("mat[1]={%d, %d, %d}\n", mat[1][0], mat[1][1], mat[1][2]);
  printf("mat[2]={%d, %d, %d}\n", mat[2][0], mat[2][1], mat[2][2]);

  return 0;
}

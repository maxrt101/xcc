use stdc::printf;

struct Test {
  x: i32;
  y: i32;
}

fn main() -> i32 {
  var t = Test { x: 42, y: 155 };
  printf("t={%d, %d}\n", t.x, t.y);

  var a = [i32] { 1, 2, 3, 4 };
  printf("a={%d, %d, %d, %d}\n", a[0], a[1], a[2], a[3]);

  var x: i32 = 42;
  var y: i32 = 69;

  var ap = [i32*] { &x, &y };
  printf("*ap[0]=%d *ap[1]=%d\n", *ap[0], *ap[1]);

  var mat = [i32[]] {
    i32 {1, 2, 3},
    i32 {4, 5, 6},
    i32 {7, 8, 9}
  };

  printf("mat[0]={%d, %d, %d}\n", mat[0][0], mat[0][1], mat[0][2]);
  printf("mat[1]={%d, %d, %d}\n", mat[1][0], mat[1][1], mat[1][2]);
  printf("mat[2]={%d, %d, %d}\n", mat[2][0], mat[2][1], mat[2][2]);

  return 0;
}

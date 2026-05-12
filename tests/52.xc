use stdc::{printf, jmp_buf, setjmp, longjmp};

struct Test {
  x: i32;
  y: i32;
}

fn main() -> i32 {
  var t = Test { x: 42, y: 155 };
  printf("t={%d, %d}\n", t.x, t.y);

  var a = i32 { 1, 2, 3, 4 };
  printf("a={%d, %d, %d, %d}\n", a[0], a[1], a[2], a[3]);

  return 0;
}

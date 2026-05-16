use stdc::printf;

struct Test {
  x: i32;
  y: i32;

  fn new(x: i32, y: i32) -> Test {
    Test { x, y }
  }
}

struct Test2 {
  t: Test;
  z: i32;

  fn new(x: i32, y: i32, z: i32) -> Test2 {
    Test2 { t: Test { x, y }, z }
  }
}

fn main() -> i32 {
  var t0: [i32, u8*] = [] { 69, "test2" };

  var [t0_0, t0_1] = t0;
  printf("t0 %d %s\n", t0_0, t0_1);

  var t1: [i32, [i32, u8*]] = [] { 69, [] { 42, "test2" } };

  var [t1_0_0, [t1_1_0, t1_1_1]] = t1;
  printf("t1 %d %d %s\n", t1_0_0, t1_1_0, t1_1_1);

  var [t_x, t_y] = Test::new(42, 69);
  printf("t %d %d\n", t_x, t_y);

  var t2 = Test2::new(11, 22, 33);

  var [[t2_t_x, t2_t_y], t2_z] = t2;
  printf("t2 %d %d %d\n", t2_t_x, t2_t_y, t2_z);

  var a0 = i32 {1, 2, 3};

  var [a0_0, a0_1, a0_2] = a0;
  printf("a0 %d %d %d\n", a0_0, a0_1, a0_2);

  var a1 = i32 {81, 72, 63, 54};

  var [a1_0, a1_1, _] = a1;
  printf("a1 %d %d\n", a1_0, a1_1);

  var m0: i32[2][2] = [] { [] {0, 1}, [] {2, 3} };

  var [[m0_0_0, m0_0_1], [m0_1_0, m0_1_1]] = m0;
  printf("m0 %d %d %d %d\n", m0_0_0, m0_0_1, m0_1_0, m0_1_1);

  return 0;
}

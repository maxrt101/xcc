use stdc;

fn test(x: i32) -> i32 {
  x * x
}

fn take_fn(f: fn (i32) -> i32, x: i32) {
  println!("take_fn({}): {}", x, f(x));
}

fn main() -> i32 {
  var ctx: i32 = 0;

  var f1 = test;

  println!("f1={} f1(4)={}", f1[0], f1(4));

  var f2 = fn [&ctx] (x: i32) -> i32 {
    *ctx = 42;
    x * x
  };

  println!("f2(4)={} ctx={}", f2(4), ctx);

  take_fn(f1, 5);
  take_fn(f2, 6);

  ctx = 4;

  var ctx2: i32 = 0;

  # Knowing that fat function pointer is {callee*, closure*}, and the internal
  # structure of closure, it is possible to modify already captured values
  # after the fact
  *(f2[1] as i32**) = &ctx2;

  println!("f2(5)={} ctx={} ctx2={}", f2(5), ctx, ctx2);

  return 0;
}

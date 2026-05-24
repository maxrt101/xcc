use stdc;

fn main() -> i32 {
  var i: i32 = 5;

  var iter_count: i32 = 0;

  while (i) {
    i -= 1;
    iter_count += 1;
  }

  println!("while1 i={} iter={}", i, iter_count);

  i = 0;
  iter_count = 0;

  while (i < 5) {
    i += 1;
    iter_count += 1;
  }

  println!("while2 i={} iter={}", i, iter_count);

  return 0;
}

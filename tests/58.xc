use stdc::{printf, puts};

fn main() -> i32 {
  print!("abcdef");
  puts("\n");

  var t0: [i32, u8*] = [] { 42, "tuple" };
  var ptr = &t0;

  println!("{04} {} {.02} {10} {x} {T} {016}", 100, "test123", 15.4, "123", 64, t0, ptr);

  return 0;
}

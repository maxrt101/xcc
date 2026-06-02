mod core::panic;

use stdc;

#
# Triggers a panic
#
macro panic(message) {
  core::panic_handler(message);
  unreachable!();
}

#
# Default panic handler
#
# TODO: Make a vararg macro, that will pass args to weak panic_handler
#
[weak, noreturn]
fn panic_handler(message: i8*) {
  println!("PANIC: {}", message);
  stdc::abort();
}

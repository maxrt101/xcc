mod mod31_2;

extern fn printf(fmt: i8*, ...): i32;

fn do_stuff2(x: i32) {
  printf("mod31_2::do_stuff2 %d\n", x);
}
mod mod29;

extern fn printf(fmt: i8*, ...): i32;

struct Doer {
  s: u8*;

  fn do(self) {
    printf("Doer::do %s\n", self->s);
  }
}

fn do_stuff(s: u8*) {
  printf("do_stuff: %s\n", s);
}

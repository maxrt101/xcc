mod mod30;

extern fn printf(fmt: i8*, ...): i32;

mod test {
  struct Doer {
    s: u8*;

    fn do(self) {
      printf("test::Doer::do %s\n", self->s);
    }
  }

  fn do_stuff(s: u8*) {
    printf("test::do_stuff: %s\n", s);
  }
}

fn do_stuff(s: u8*) {
  printf("do_stuff: %s\n", s);
}

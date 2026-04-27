extern fn printf(fmt: i8*, ...): i32;

mod test {
  struct Doer {
    s: u8*;

    fn do(self) {
      printf("Doer::do %s\n", self->s);
    }
  }

  fn do_stuff(s: u8*) {
    printf("do_stuff: %s\n", s);
  }
}

fn main(): i32 {
  test::do_stuff("42");

  var doer: test::Doer;
  doer.s = "69";
  doer.do();

  return 0;
}

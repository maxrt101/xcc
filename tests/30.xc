use mod30;

fn main() -> i32 {
  mod30::do_stuff("42");
  mod30::test::do_stuff("420");

  var doer: mod30::test::Doer;
  doer.s = "69";
  doer.do();

  return 0;
}
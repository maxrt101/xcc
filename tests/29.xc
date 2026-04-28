use mod29;

fn main() -> i32 {
  mod29::do_stuff("42");

  var doer: mod29::Doer;
  doer.s = "69";
  doer.do();

  return 0;
}
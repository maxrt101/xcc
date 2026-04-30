extern fn printf(fmt: u8*, ...) -> i32;

fn main() -> i32 {
  printf("inc=%d\n", inc!(1));
  printf("dec=%d\n", dec!(5));

  printf("add=%d\n", add!(1, 2));
  printf("sub=%d\n", sub!(6, 1));
  printf("mul=%d\n", mul!(2, 4));
  printf("div=%d\n", div!(6, 2));

  printf("eq_int_1=%d\n", eq!(3, 3));
  printf("eq_int_0=%d\n", eq!(3, 1));
  printf("eq_str_1=%d\n", eq!("abc1", "abc1"));
  printf("eq_str_0=%d\n", eq!("abc1", "abc2"));

  printf("ne_int_0=%d\n", ne!(3, 3));
  printf("ne_int_1=%d\n", ne!(3, 1));
  printf("ne_str_0=%d\n", ne!("abc1", "abc1"));
  printf("ne_str_1=%d\n", ne!("abc1", "abc2"));

  printf("lt_1=%d\n", lt!(1, 2));
  printf("lt_0=%d\n", lt!(2, 1));

  printf("le_1=%d\n", le!(1, 2));
  printf("le_eq=%d\n", le!(2, 2));
  printf("le_0=%d\n", le!(2, 1));

  printf("gt_1=%d\n", gt!(2, 1));
  printf("gt_0=%d\n", gt!(1, 2));

  printf("ge_1=%d\n", ge!(2, 1));
  printf("ge_eq=%d\n", ge!(2, 2));
  printf("ge_0=%d\n", ge!(1, 2));

  printf("and_1=%d\n", and!(1, 1));
  printf("and_0=%d\n", and!(1, 0));

  printf("or_1=%d\n", or!(1, 0));
  printf("or_0=%d\n", or!(0, 0));

  printf("int=%d\n", int!("0x2a"));

  return 0;
}

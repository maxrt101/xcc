use stdc::printf;

enum TestEnum : i8 {
  A = 0,
  B = 1,
  C = 5,
  D,
  E,
  F = TestEnum::C + 7,
  G
}

mod test {
  enum ModEnum : i8 {
    A = 0,
    B = 1,
    C = 5,
    D,
    E,
    F = ModEnum::C + 7,
    G
  }

  fn getEnumF() -> i8 {
    return ModEnum::F;
  }
}

fn main() -> i32 {
  printf("TestEnum: %d %d %d %d %d %d %d\n", TestEnum::A, TestEnum::B, TestEnum::C, TestEnum::D, TestEnum::E, TestEnum::F, TestEnum::G);
  printf("ModEnum: %d %d %d %d %d %d %d\n", test::ModEnum::A, test::ModEnum::B, test::ModEnum::C, test::ModEnum::D, test::ModEnum::E, test::ModEnum::F, test::ModEnum::G);
  printf("F=%d\n", test::getEnumF());
  return 0;
}

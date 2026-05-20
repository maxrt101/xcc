use stdc;

enum TestEnum : i8 {
  A = 0,
  B = 1,
  C = 5,
  D,
  E,
  F = TestEnum::C + 7,
  G,

  fn toString(self) -> i8* {
    match (*self) {
      TestEnum::A -> "A",
      TestEnum::B -> "B",
      TestEnum::C -> "C",
      TestEnum::D -> "D",
      TestEnum::E -> "E",
      TestEnum::F | TestEnum::G -> "F or G",
      _ -> "?"
    }
  }
}

fn main() -> i32 {
  println!("A::toString: '{}'", (TestEnum::A).toString());
  println!("B::toString: '{}'", (TestEnum::B).toString());
  println!("F::toString: '{}'", (TestEnum::F).toString());
  println!("G::toString: '{}'", (TestEnum::G).toString());
  println!("42::toString: '{}'", (42 as TestEnum).toString());

  return 0;
}

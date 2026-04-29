extern fn printf(fmt: u8*, ...) -> i32;

macro genVectorType(name, typ) {
  struct name {
    data: typ*;
    size: usize;
    capacity: usize;
  }
}

macro genPowFn(name, typ) {
  fn name(x: typ) -> typ {
    return x * x;
  }
}

genVectorType!(Veci32, i32)

genPowFn!(powi, i32)
genPowFn!(powf, f64)

fn main() -> i32 {
  printf("powi=%d\n", powi(4));
  printf("powf=%.4f\n", powf(4.25));

  # No typeof, sizeof or type comparison, so resort to a very hacky way to generated type
  # TODO: Rewrite this, when type comparison is implemented
  var x: i32 = 0xFFFFFFFF;
  var v: Veci32;
  v.data = &x;
  printf("*v.data=%X\n", *v.data);

  return 0;
}

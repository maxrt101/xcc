use stdc;

struct Test<T, P = T*, PP = P*> {
  value: T;
  ptr: P;
  ptr2: PP;
}

fn main() -> i32 {
  var x: i32 = 42;
  var p = &x;

  var a = Test::<i32> { value: x, ptr: &x, ptr2: &p };

  println!("a {} {} {}", a.value, *a.ptr, **a.ptr2);

  return 0;
}

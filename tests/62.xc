use stdc;

struct Container<T> {
  ptr: T*;

  fn getErasedPtr(self) -> void* {
    return self->ptr;
  }
}

struct Test<T> {
  container: Container<T>;
}

struct TestPtr<T> {
  container: Container<T>*;
}

fn main() -> i32 {
  var ctr1: Container<i32>;
  var x: i32 = 42;
  ctr1.ptr = &x;

  println!("*ctr1.ptr={} getErasedPtr={}", *ctr1.ptr, ctr1.getErasedPtr());

  var ctr2: Container<i8*>;
  ctr2.ptr = "test str";

  println!("ctr2.ptr='{%s}' getErasedPtr={}", ctr2.ptr, ctr2.getErasedPtr());

  var t: Test<i32>;
  t.container = ctr1;
  println!("*t.container.ptr={}", *t.container.ptr);

  var tp: TestPtr<i32>;
  tp.container = &ctr1;
  println!("*tp.container->ptr={}", *(tp.container->ptr));

  return 0;
}

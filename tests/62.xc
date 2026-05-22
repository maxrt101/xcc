use stdc;

struct Container<T> {
  ptr: T*;

  fn new(ptr: T*) -> Container {
    Container { ptr }
  }

  fn getErasedPtr(self) -> void* {
    return self->ptr;
  }
}

struct Test<T> {
  container: Container<T>;

  fn new(container: Container<T>) -> Test {
    Test { container }
  }
}

struct TestPtr<T> {
  container: Container<T>*;

  fn new(container: Container<T>*) -> TestPtr {
    TestPtr { container }
  }
}

fn main() -> i32 {
  var x: i32 = 42;

  var ctr1 = Container<i32>::new(&x);
  println!("*ctr1.ptr={} getErasedPtr={}", *ctr1.ptr, ctr1.getErasedPtr());

  var ctr2 = Container<i8*>::new("test str");
  println!("ctr2.ptr='{%s}' getErasedPtr={}", ctr2.ptr, ctr2.getErasedPtr());

  var t = Test<i32>::new(ctr1);
  println!("*t.container.ptr={}", *t.container.ptr);

  var tp = TestPtr<i32>::new(&ctr1);
  println!("*tp.container->ptr={}", *(tp.container->ptr));

  return 0;
}

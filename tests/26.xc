extern fn printf(fmt: i8*, ...): i32;
extern fn malloc(size: u32): u8*;
extern fn free(ptr: u8*): void;
extern fn strlen(ptr: u8*): u32;

struct Counter {
  value: i32;

  fn init(self): void {
    self->value = 1;
  }
}

struct Context {
  counter: Counter;

  fn inc(self): void {
    self->counter.value += 1;
  }

  fn init(self): void {
    self->counter.init();
  }
}

fn main(): i32 {
  var ctx: Context;

  ctx.init();

  ctx.inc();
  ctx.inc();
  ctx.inc();

  printf("%d\n", ctx.counter.value);

  return 0;
}

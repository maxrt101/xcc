extern fn printf(fmt: i8*, ...): i32;

struct buffer_t {
  data: i8*;
  size: u32;
}

struct context_t {
  status: u32;
  buf: buffer_t;
}

fn context_init(ctx: context_t*): i32 {
  ctx->buf.data = "Hello, World!";

  ctx->status = 1;

  return 0;
}

fn main(): i32 {
  var s: context_t;

  s.status = 0;

  context_init(&s);

  printf("%p %s\n", s.buf.data, s.buf.data);

  return 0;
}

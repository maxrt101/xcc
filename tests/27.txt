extern fn printf(fmt: i8*, ...): i32;

fn test(): void {
  var x: i8 = 15;
  var y: i8* = &x;
  var z: i8** = &y;

  printf("Raw Values:   %u %u %u\n", x, y, z);
  printf("Addresses:    %p %p %p\n", &x, &y, &z);
  printf("Dereferences: %u %u %u\n", x, *y, **z);

}

fn main(): i32 {
  test();

  return 0;
}
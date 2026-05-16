use stdc::printf;

fn ret_tuple() -> [i32, u8*] {
  [] { 42, "tuple" }
}

fn main() -> i32 {
  var t0 = ret_tuple();
  printf("t0={%d: %s, %s: %s}\n",
    t0[0], str!(typeof!( t0[0] )),
    t0[1], str!(typeof!( t0[1] ))
  );

  var t1: [i32, u8*] = [] { 69, "test2" };
  printf("t1={%d: %s, %s: %s}\n",
    t1[0], str!(typeof!( t1[0] )),
    t1[1], str!(typeof!( t1[1] ))
  );

  var t2: [i32, [i32, u8*]] = [] { 69, [] { 42, "test2" } };
  printf("t2={%d, {%d, %s}}\n", t2[0], t2[1][0], t2[1][1]);

  return 0;
}

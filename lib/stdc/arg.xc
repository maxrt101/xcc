mod arg;

struct va_list {
    gp_offset:         u32;
    fp_offset:         u32;
    overflow_arg_area: u8*;
    reg_save_area:     u8*;
}

# LLVM intrinsic
[alias("llvm.va_start.p0")]
fn __va_start(ap: u8*);

# LLVM intrinsic
[alias("llvm.va_end.p0")]
fn __va_end(ap: u8*);

macro va_start(ap, v) {
  stdc::arg::__va_start(&ap);
}

macro va_end(ap) {
  stdc::arg::__va_end(&ap);
}

macro va_arg(ap, t) {
  {
    var result: t;

    if (ap.gp_offset < 48) {
      result = *( (ap.reg_save_area + ap.gp_offset) as t* );
      ap.gp_offset += 8;
    } else {
      result = *( ap.overflow_arg_area as t* );
      ap.overflow_arg_area += 8;
    }

    result
  }
}

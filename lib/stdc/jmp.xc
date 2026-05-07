mod jmp;

type jmp_buf = u8[40];

# The setjmp() function saves various information about the calling
# environment (typically, the stack pointer, the instruction
# pointer, possibly the values of other registers and the signal
# mask) in the buffer env for later use by longjmp().  In this case,
# setjmp() returns 0.
#
[alias("setjmp")]
fn setjmp(buf: u8*) -> i32;

# The longjmp() function uses the information saved in env to
# transfer control back to the point where setjmp() was called and
# to restore ("rewind") the stack to its state at the time of the
# setjmp() call. In addition, and depending on the implementation
# (see NOTES and HISTORY), the values of some other registers and
# the process signal mask may be restored to their state at the time
# of the setjmp() call.
# Following a successful longjmp(), execution continues as if
# setjmp() had returned for a second time. This "fake" return can
# be distinguished from a true setjmp() call because the "fake"
# return returns the value provided in val. If the programmer
# mistakenly passes the value 0 in val, the "fake" return will
# instead return 1.
#
[alias("longjmp")]
fn longjmp(buf: u8*, val: i32);

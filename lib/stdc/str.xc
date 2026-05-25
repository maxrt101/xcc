mod stdc::str;

# The memcpy() function copies n bytes from memory area src to
# memory area dest. The memory areas must not overlap. Use
# memmove(3) if the memory areas do overlap.
#
# @param dest Where to copy data
# @param src  Data to copy
# @param size Size of data
# @returns dest
#
[alias(memcpy)]
fn memcpy(dest: void*, src: void*, size: usize) -> void*;

# The memcmp() function compares the first n bytes (each interpreted
# as unsigned char) of the memory areas s1 and s2.
#
# @param s1   Buffer #1
# @param s2   Buffer #2
# @param size Size to compare
# @returns The memcmp() function returns an integer less than, equal to, or
#          greater than zero if the first n bytes of s1 is found,
#          respectively, to be less than, to match, or be greater than the
#          first n bytes of s2.
#          For a nonzero return value, the sign is determined by the sign of
#          the difference between the first pair of bytes (interpreted as
#          unsigned char) that differ in s1 and s2.
#          If n is zero, the return value is zero.
#
[alias(memcmp)]
fn memcmp(s1: void*, s2: void*, size: usize) -> i32;

# The memset() function fills the first n bytes of the memory area
# pointed to by s with the constant byte c.
#
# @param buf  Buffer to set each byte in
# @param val  Byte to set
# @param size Size of buf
# @returns dest
#
[alias(memset)]
fn memset(buf: void*, val: i32, size: usize) -> void*;

# The strlen() function calculates the length of the string pointed
# to by s, excluding the terminating null byte ('\0').
#
# @param str String to calculate length of
# @returns Size of str
#
[alias(strlen)]
fn strlen(str: u8*) -> u32;

# Copy the string pointed to by src, into a
# string at the buffer pointed to by dst.  The programmer is
# responsible for allocating a destination buffer large
# enough, that is, strlen(src) + 1.
#
# @param dest String to copy into
# @param src  String to copy from
# @returns dest
#
[alias(strcpy)]
fn strcpy(dest: u8*, str: u8*) -> u8*;

# This function catenates the string pointed to by src, after
# the string pointed to by dst (overwriting its terminating
# null byte).  The programmer is responsible for allocating a
# destination buffer large enough, that is, strlen(dst) +
# strlen(src) + 1.
#
# @param dest String to copy into
# @param src  String to copy from
# @returns dest
#
[alias(strcat)]
fn strcat(dest: u8*, str: u8*) -> u8*;

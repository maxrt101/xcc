mod io;

# Alias to opaque FILE struct
type FILE = void;

type off_t = usize;
type pos_t = usize;

const SEEK_SET: u32 = 0;
const SEEK_CUR: u32 = 1;
const SEEK_END: u32 = 2;

const EOF: i32 = -1;

# The fopen() function opens the file whose name is the string
# pointed to by path and associates a stream with it.
# The argument mode points to a string beginning with one of the
# following sequences (possibly followed by additional characters,
# as described below):
#       r      Open text file for reading.  The stream is positioned at
#              the beginning of the file.
#       r+     Open for reading and writing.  The stream is positioned at
#              the beginning of the file.
#       w      Truncate file to zero length or create text file for
#              writing.  The stream is positioned at the beginning of the
#              file.
#       w+     Open for reading and writing.  The file is created if it
#              does not exist, otherwise it is truncated.  The stream is
#              positioned at the beginning of the file.
#       a      Open for appending (writing at end of file).  The file is
#              created if it does not exist.  The stream is positioned at
#              the end of the file.
#       a+     Open for reading and appending (writing at end of file).
#              The file is created if it does not exist.  Output is always
#              appended to the end of the file.  POSIX is silent on what
#              the initial read position is when using this mode.  For
#              glibc, the initial file position for reading is at the
#              beginning of the file, but for Android/BSD/MacOS, the
#              initial file position for reading is at the end of the
#              file.
#
# @param path Path to file
# @param mode File mode
# @returns A valid pointer to FILE on success, NULL otherwise
#
[alias(fopen)]
fn fopen(path: u8*, mode: u8*) -> FILE*;

# The fclose() function flushes the stream pointed to by stream
# (writing any buffered output data using fflush(3)) and closes the
# underlying file descriptor.
#
# @param file Valid (and opened) file handle
# @returns 0 on success
#
[alias(fclose)]
fn fclose(file: FILE*) -> i32;

# The function fread() reads `n` items of data, each `size` bytes long,
# from the stream pointed to by `file`, storing them at the location
# given by `buf`.
#
# @param buf  Buffer to write read data into
# @param size Size of one element to read
# @param n    Count of elements
# @param file Valid file handle
# @returns Number (count) of items read
#
[alias(fread)]
fn fread(buf: void*, size: usize, n: usize, file: FILE*) -> usize;

# The function fwrite() writes `n` items of data, each `size` bytes
# long, to the stream pointed to by `file`, obtaining them from the
# location given by `buf`.
#
# @param buf  Buffer to read data from
# @param size Size of one element to write
# @param n    Count of elements
# @param file Valid file handle
# @returns Number (count) of items written
[alias(fwrite)]
fn fwrite(buf: void*, size: usize, n: usize, file: FILE*) -> usize;

# The fgetpos() and fsetpos() functions are alternate interfaces
# equivalent to ftell() and fseek() (with whence set to SEEK_SET),
# setting and storing the current value of the file offset into or
# from the object referenced by pos.  On some non-UNIX systems, an
# fpos_t object may be a complex object and these routines may be
# the only way to portably reposition a text stream.
#
# @param file Valid file handle
# @param pos  Pointer to pos_t, where return should be put
#
[alias(fgetpos)]
fn fgetpos(file: FILE*, pos: pos_t*) -> u32;

# See @ref fgetpos
#
# @param file Valid file handle
# @param pos  Pointer to pos_t, from where file position should be set
#
[alias(fsetpos)]
fn fsetpos(file: FILE*, pos: pos_t*) -> u32;

# The fseek() function sets the file position indicator for the
# stream pointed to by `file`. The new position, measured in bytes,
# is obtained by adding `offset` bytes to the position specified by
# `origin`. If whence is set to SEEK_SET, SEEK_CUR, or SEEK_END, the
# offset is relative to the start of the file, the current position
# indicator, or end-of-file, respectively. A successful call to the
# fseek() function clears the end-of-file indicator for the stream
# and undoes any effects of the ungetc(3) function on the same
# stream.
#
# @param file   Valid file handle
# @param offset Bytes to add to retrieved position
# @param origin Specified from where to seek
# @returns 0 on success
#
[alias(fseek)]
fn fseek(file: FILE*, offset: u32, origin: u32) -> u32;

# The ftell() function obtains the current value of the file
# position indicator for the stream pointed to by stream.
# See @ref fseek
#
# @param file Valid file handle
# @returns current position in file
#
[alias(ftell)]
fn ftell(file: FILE*) -> u32;

# The rewind() function sets the file position indicator for the
# stream pointed to by stream to the beginning of the file.
#
# @param file Valid file handle
#
[alias(rewind)]
fn rewind(file: FILE*);

# remove() deletes a name from the filesystem.  It calls unlink(2)
# for files, and rmdir(2) for directories.
# If the removed name was the last link to a file and no processes
# have the file open, the file is deleted and the space it was using
# is made available for reuse.
# If the name was the last link to a file, but any processes still
# have the file open, the file will remain in existence until the
# last file descriptor referring to it is closed.
# If the name referred to a symbolic link, the link is removed.
# If the name referred to a socket, FIFO, or device, the name is
# removed, but processes which have the object open may continue to
# use it.
#
# @param path Path to object that has to be removed
#
[alias(remove)]
fn remove(path: u8*);

# rename() renames a file, moving it between directories if
# required.  Any other hard links to the file (as created using
# link(2)) are unaffected.  Open file descriptors for oldpath are
# also unaffected.
# Various restrictions determine whether or not the rename operation
# succeeds: see ERRORS below.
# If newpath already exists, it will be atomically replaced, so that
# there is no point at which another process attempting to access
# newpath will find it missing.  However, there will probably be a
# window in which both oldpath and newpath refer to the file being
# renamed.
# If oldpath and newpath are existing hard links referring to the
# same file, then rename() does nothing, and returns a success
# status.
# If newpath exists but the operation fails for some reason,
# rename() guarantees to leave an instance of newpath in place.
# oldpath can specify a directory.  In this case, newpath must
# either not exist, or it must specify an empty directory.
# If oldpath refers to a symbolic link, the link is renamed; if
# newpath refers to a symbolic link, the link will be overwritten.
#
# @param old Old path
# @param new New path
#
[alias(rename)]
fn rename(old: u8*, new: u8*);

# fgetc() reads the next character from stream and returns it as an
# unsigned char cast to an int, or EOF on end of file or error.
#
# @param file Valid file handle
# @returns char on success, EOF otherwise
#
[alias(fgetc)]
fn fgetc(file: FILE*) -> i32;

# fgets() reads in at most one less than `size` characters from `file`
# and stores them into the buffer pointed to by `buf`. Reading stops
# after an EOF or a newline.  If a newline is read, it is stored
# into the buffer. A terminating null byte ('\0') is stored after
# the last character in the buffer.
#
# @param buf  Buffer to read into
# @param size Size to read
# @param file Valid file handle
# @returns buf on success
#
[alias(fgets)]
fn fgets(buf: u8*, size: i32, file: FILE*) -> u8*;

# getc() is equivalent to fgetc(3), except for the BUGS (see below).
# Use fgetc(3) instead.
#
# @param file Valid file handle
# @returns char
#
[alias(getc)]
fn getc(file: FILE*) -> i32;

# gets() reads a line from stdin into the buffer pointed to by `buf`
# until either a terminating newline or EOF, which it replaces with
# a null byte ('\0'). No check for buffer overrun is performed (see
# BUGS below).
#
# @param buf Buffer to write into
# @returns buf on success
#
[alias(gets)]
fn gets(buf: u8*) -> u8*;

# getchar() is equivalent to fgetc(stdin).
# See @ref getc
#
# @returns Char
#
[alias(getchar)]
fn getchar() -> i32;

# fputc() writes the character `ch`, cast to an unsigned char, to
# `file`.
#
# @param ch   Char to write
# @param file Valid file handle
# @returns ch on success
#
[alias(fputc)]
fn fputc(ch: i32, file: FILE*) -> i32;

# fputs() writes the string `str` to `file`, without its terminating
# null byte ('\0').
#
# @param str  String to write
# @param file Valid file handle
# @returns non-negative number on success
#
[alias(fputs)]
fn fputs(str: u8*, file: FILE*) -> i32;

# putc() is equivalent to fputc() except that it may be implemented
# as a macro which evaluates stream more than once.
#
# @param ch   Char to write
# @param file Valid file handle
# @returns `ch` on success
#
[alias(putc)]
fn putc(ch: i32, file: FILE*) -> i32;

# puts() writes the string s and a trailing newline to stdout.
#
# @param str String to write
# @returns non-negative number on success
#
[alias(puts)]
fn puts(str: u8*) -> i32;

# putchar(c) is equivalent to putc(c, stdout).
#
# @param ch Char to write
# @returns `ch` on success
#
[alias(putchar)]
fn putchar(ch: i32) -> i32;

# The functions in the printf() family produce output according to a
# format as described below. The functions printf() and vprintf()
# write output to stdout, the standard output stream; fprintf() and
# vfprintf() write output to the given output stream; sprintf(),
# snprintf(), vsprintf(), and vsnprintf() write to the character
# string str.
#
# Format:
# %[argument$][flags][width][.precision][length modifier]conversion
#
# Flag characters
#       The character % is followed by zero or more of the following
#       flags:
#
#       #      The value should be converted to an "alternate form".  For
#              o conversions, the first character of the output string is
#              made zero (by prefixing a 0 if it was not zero already).
#              For x and X conversions, a nonzero result has the string
#              "0x" (or "0X" for X conversions) prepended to it.  For a,
#              A, e, E, f, F, g, and G conversions, the result will always
#              contain a decimal point, even if no digits follow it
#              (normally, a decimal point appears in the results of those
#              conversions only if a digit follows).  For g and G
#              conversions, trailing zeros are not removed from the result
#              as they would otherwise be.  For m, if errno contains a
#              valid error code, the output of strerrorname_np(errno) is
#              printed; otherwise, the value stored in errno is printed as
#              a decimal number.  For other conversions, the result is
#              undefined.
#
#       0      The value should be zero padded.  For d, i, o, u, x, X, a,
#              A, e, E, f, F, g, and G conversions, the converted value is
#              padded on the left with zeros rather than blanks.  If the 0
#              and - flags both appear, the 0 flag is ignored.  If a
#              precision is given with an integer conversion (d, i, o, u,
#              x, and X), the 0 flag is ignored.  For other conversions,
#              the behavior is undefined.
#
#       -      The converted value is to be left adjusted on the field
#              boundary.  (The default is right justification.)  The
#              converted value is padded on the right with blanks, rather
#              than on the left with blanks or zeros.  A - overrides a 0
#              if both are given.
#
#       ' '    (a space) A blank should be left before a positive number
#              (or empty string) produced by a signed conversion.
#
#       +      A sign (+ or -) should always be placed before a number
#              produced by a signed conversion.  By default, a sign is
#              used only for negative numbers.  A + overrides a space if
#              both are used.
#
#       The five flag characters above are defined in the C99 standard.
#       POSIX specifies one further flag character.
#
#       '      For decimal conversion (i, d, u, f, F, g, G) the output is
#              to be grouped with thousands' grouping characters as a non-
#              monetary quantity.  Misleadingly, this isn't necessarily
#              every thousand: for example Karbi ("mjw_IN"), groups its
#              digits into 3 once, then 2 repeatedly.  Compare locale(7)
#              grouping and thousands_sep, contrast with
#              mon_grouping/mon_thousands_sep and strfmon(3).  This is a
#              no-op in the default "C" locale.
#
#       glibc 2.2 adds one further flag character.
#
#       I      For decimal integer conversion (i, d, u) the output uses
#              the locale's alternative output digits, if any.  For
#              example, since glibc 2.2.3 this will give Arabic-Indic
#              digits in the Persian ("fa_IR") locale.
#
# Field width
#       An optional decimal digit string (with nonzero first digit)
#       specifying a minimum field width.  If the converted value has
#       fewer characters than the field width, it will be padded with
#       spaces on the left (or right, if the left-adjustment flag has been
#       given).  Instead of a decimal digit string one may write "*" or
#       "*m$" (for some decimal integer m) to specify that the field width
#       is given in the next argument, or in the m-th argument,
#       respectively, which must be of type int.  A negative field width
#       is taken as a '-' flag followed by a positive field width.  In no
#       case does a nonexistent or small field width cause truncation of a
#       field; if the result of a conversion is wider than the field
#       width, the field is expanded to contain the conversion result.
#
# Precision
#       An optional precision, in the form of a period ('.') followed by
#       an optional decimal digit string.  Instead of a decimal digit
#       string one may write "*" or "*m$" (for some decimal integer m) to
#       specify that the precision is given in the next argument, or in
#       the m-th argument, respectively, which must be of type int.  If
#       the precision is given as just '.', the precision is taken to be
#       zero.  A negative precision is taken as if the precision were
#       omitted.  This gives the minimum number of digits to appear for d,
#       i, o, u, x, and X conversions, the number of digits to appear
#       after the radix character for a, A, e, E, f, and F conversions,
#       the maximum number of significant digits for g and G conversions,
#       or the maximum number of characters to be printed from a string
#       for s and S conversions.
#
# Length modifier
#       Here, "integer conversion" stands for d, i, o, u, x, or X
#       conversion.
#
#       hh     A following integer conversion corresponds to a signed char
#              or unsigned char argument, or a following n conversion
#              corresponds to a pointer to a signed char argument.
#
#       h      A following integer conversion corresponds to a short or
#              unsigned short argument, or a following n conversion
#              corresponds to a pointer to a short argument.
#
#       l      (ell) A following integer conversion corresponds to a long
#              or unsigned long argument, or a following n conversion
#              corresponds to a pointer to a long argument, or a following
#              c conversion corresponds to a wint_t argument, or a
#              following s conversion corresponds to a pointer to wchar_t
#              argument.  On a following a, A, e, E, f, F, g, or G
#              conversion, this length modifier is ignored (C99; not in
#              SUSv2).
#
#       ll     (ell-ell).  A following integer conversion corresponds to a
#              long long or unsigned long long argument, or a following n
#              conversion corresponds to a pointer to a long long
#              argument.
#
#       q      A synonym for ll.  This is a nonstandard extension, derived
#              from BSD; avoid its use in new code.
#
#       L      A following a, A, e, E, f, F, g, or G conversion
#              corresponds to a long double argument.  (C99 allows %LF,
#              but SUSv2 does not.)
#
#       j      A following integer conversion corresponds to an intmax_t
#              or uintmax_t argument, or a following n conversion
#              corresponds to a pointer to an intmax_t argument.
#
#       z      A following integer conversion corresponds to a size_t or
#              ssize_t argument, or a following n conversion corresponds
#              to a pointer to a size_t argument.
#
#       Z      A nonstandard synonym for z that predates the appearance of
#              z.  Do not use in new code.
#
#       t      A following integer conversion corresponds to a ptrdiff_t
#              argument, or a following n conversion corresponds to a
#              pointer to a ptrdiff_t argument.
#
#       SUSv3 specifies all of the above, except for those modifiers
#       explicitly noted as being nonstandard extensions.  SUSv2 specified
#       only the length modifiers h (in hd, hi, ho, hx, hX, hn) and l (in
#       ld, li, lo, lx, lX, ln, lc, ls) and L (in Le, LE, Lf, Lg, LG).
#
#       As a nonstandard extension, the GNU implementations treats ll and
#       L as synonyms, so that one can, for example, write llg (as a
#       synonym for the standards-compliant Lg) and Ld (as a synonym for
#       the standards compliant lld).  Such usage is nonportable.
#
#   Conversion specifiers
#       A character that specifies the type of conversion to be applied.
#       The conversion specifiers and their meanings are:
#
#       d, i   The int argument is converted to signed decimal notation.
#              The precision, if any, gives the minimum number of digits
#              that must appear; if the converted value requires fewer
#              digits, it is padded on the left with zeros.  The default
#              precision is 1.  When 0 is printed with an explicit
#              precision 0, the output is empty.
#
#       o, u, x, X
#              The unsigned int argument is converted to unsigned octal
#              (o), unsigned decimal (u), or unsigned hexadecimal (x and
#              X) notation.  The letters abcdef are used for x
#              conversions; the letters ABCDEF are used for X conversions.
#              The precision, if any, gives the minimum number of digits
#              that must appear; if the converted value requires fewer
#              digits, it is padded on the left with zeros.  The default
#              precision is 1.  When 0 is printed with an explicit
#              precision 0, the output is empty.
#
#       e, E   The double argument is rounded and converted in the style
#              [-]d.ddde±dd where there is one digit (which is nonzero if
#              the argument is nonzero) before the decimal-point character
#              and the number of digits after it is equal to the
#              precision; if the precision is missing, it is taken as 6;
#              if the precision is zero, no decimal-point character
#              appears.  An E conversion uses the letter E (rather than e)
#              to introduce the exponent.  The exponent always contains at
#              least two digits; if the value is zero, the exponent is 00.
#
#       f, F   The double argument is rounded and converted to decimal
#              notation in the style [-]ddd.ddd, where the number of
#              digits after the decimal-point character is equal to the
#              precision specification.  If the precision is missing, it
#              is taken as 6; if the precision is explicitly zero, no
#              decimal-point character appears.  If a decimal point
#              appears, at least one digit appears before it.
#
#              (SUSv2 does not know about F and says that character string
#              representations for infinity and NaN may be made available.
#              SUSv3 adds a specification for F.  The C99 standard
#              specifies "[-]inf" or "[-]infinity" for infinity, and a
#              string starting with "nan" for NaN, in the case of f
#              conversion, and "[-]INF" or "[-]INFINITY" or "NAN" in the
#              case of F conversion.)
#
#       g, G   The double argument is converted in style f or e (or F or E
#              for G conversions).  The precision specifies the number of
#              significant digits.  If the precision is missing, 6 digits
#              are given; if the precision is zero, it is treated as 1.
#              Style e is used if the exponent from its conversion is less
#              than -4 or greater than or equal to the precision.
#              Trailing zeros are removed from the fractional part of the
#              result; a decimal point appears only if it is followed by
#              at least one digit.
#
#       a, A   (C99; not in SUSv2, but added in SUSv3) For a conversion,
#              the double argument is converted to hexadecimal notation
#              (using the letters abcdef) in the style [-]0xh.hhhhp±d; for
#              A conversion the prefix 0X, the letters ABCDEF, and the
#              exponent separator P is used.  There is one hexadecimal
#              digit before the radix point, and the number of digits
#              after it is equal to the precision.  The default precision
#              suffices for an exact representation of the value if an
#              exact representation in base 2 exists and otherwise is
#              sufficiently large to distinguish values of type double.
#              The digit before the radix point is unspecified for
#              nonnormalized numbers, and nonzero but otherwise
#              unspecified for normalized numbers.  The exponent, d, is
#              the appropriate exponent of 2 expressed as a decimal
#              integer; it always contains at least one digit; if the
#              value is zero, the exponent is 0.
#
#       c      If no l modifier is present, the int argument is converted
#              to an unsigned char, and the resulting character is
#              written.  If an l modifier is present, the wint_t (wide
#              character) argument is converted to a multibyte sequence by
#              a call to the wcrtomb(3) function, with a conversion state
#              starting in the initial state, and the resulting multibyte
#              string is written.
#
#       s      If no l modifier is present: the const char * argument is
#              expected to be a pointer to an array of character type
#              (pointer to a string).  Characters from the array are
#              written up to (but not including) a terminating null byte
#              ('\0'); if a precision is specified, no more than the
#              number specified are written.  If a precision is given, no
#              null byte need be present; if the precision is not
#              specified, or is greater than the size of the array, the
#              array must contain a terminating null byte.
#
#              If an l modifier is present: the const wchar_t * argument
#              is expected to be a pointer to an array of wide characters.
#              Wide characters from the array are converted to multibyte
#              characters (each by a call to the wcrtomb(3) function, with
#              a conversion state starting in the initial state before the
#              first wide character), up to and including a terminating
#              null wide character.  The resulting multibyte characters
#              are written up to (but not including) the terminating null
#              byte.  If a precision is specified, no more bytes than the
#              number specified are written, but no partial multibyte
#              characters are written.  Note that the precision determines
#              the number of bytes written, not the number of wide
#              characters or screen positions.  The array must contain a
#              terminating null wide character, unless a precision is
#              given and it is so small that the number of bytes written
#              exceeds it before the end of the array is reached.
#
#       C      (Not in C99 or C11, but in SUSv2, SUSv3, and SUSv4.)
#              Synonym for lc.  Don't use.
#
#       S      (Not in C99 or C11, but in SUSv2, SUSv3, and SUSv4.)
#              Synonym for ls.  Don't use.
#
#       p      The void * pointer argument is printed in hexadecimal (as
#              if by %#x or %#lx).
#
#       n      The number of characters written so far is stored into the
#              integer pointed to by the corresponding argument.  That
#              argument shall be an int *, or variant whose size matches
#              the (optionally) supplied integer length modifier.  No
#              argument is converted.  (This specifier is not supported by
#              the bionic C library.)  The behavior is undefined if the
#              conversion specification includes any flags, a field width,
#              or a precision.
#
#       m      (glibc extension; supported by uClibc and musl, and on
#              Android from API level 29.)  Print output of
#              strerror(errno) (or strerrorname_np(errno) in the alternate
#              form).  No argument is required.
#
#       %      A '%' is written.  No argument is converted.  The complete
#              conversion specification is '%%'.
#
# @param fmt Format string
# @returns the number of bytes printed (excluding the null byte used to end
#          output to strings)
#
[alias(printf)]
fn printf(fmt: u8*, ...) -> i32;

# Formats a string into buffer, instead of stdout
# See @ref printf
#
# @param buf Buffer to put formatted string into
# @param fmt Format string
# @returns the number of bytes printed (excluding the null byte used to end
#          output to strings)
#
[alias(sprintf)]
fn sprintf(buf: u8*, fmt: u8*, ...) -> i32;

# Formats a string into buffer, instead of stdout
# See @ref printf
#
# The functions snprintf() and vsnprintf() do not write more than
# size bytes (including the terminating null byte ('\0')).  If the
# output was truncated due to this limit, then the return value is
# the number of characters (excluding the terminating null byte)
# which would have been written to the final string if enough space
# had been available.  Thus, a return value of size or more means
# that the output was truncated.
#
# @param buf Buffer to put formatted string into
# @param fmt  Format string
# @param size Buffer size
# @returns the number of bytes printed (excluding the null byte used to end
#          output to strings)
#
[alias(snprintf)]
fn snprintf(buf: u8*, size: usize, fmt: u8*, ...) -> i32;

# Formats a string into a file, instead of stdout
# See @ref printf
#
# @param file Valid file handle
# @param fmt  Format string
# @returns the number of bytes printed (excluding the null byte used to end
#          output to strings)
#
[alias(fprintf)]
fn fprintf(file: FILE*, fmt: u8*, ...) -> i32;

# The scanf() family of functions scans formatted input like
# sscanf(3), but read from a FILE. It is very difficult to use
# these functions correctly, and it is preferable to read entire
# lines with fgets(3) or getline(3) and parse them later with
# sscanf(3) or more specialized functions such as strtol(3).
# The scanf() function reads input from the standard input stream
# stdin and fscanf() reads input from the stream pointer stream.
#
# @param fmt Scan format
# @returns On success, these functions return the number of input items
#          successfully matched and assigned; this can be fewer than provided
#          for, or even zero, in the event of an early matching failure.
#          The value EOF is returned if the end of input is reached before
#          either the first successful conversion or a matching failure
#          occurs. EOF is also returned if a read error occurs
#
[alias(scanf)]
fn scanf(fmt: u8*, ...) -> i32;

# See @ref scanf
#
# @param buf Buffer to parse
# @param fmt Scan format
#
[alias(sscanf)]
fn sscanf(buf: u8*, fmt: u8*, ...) -> i32;

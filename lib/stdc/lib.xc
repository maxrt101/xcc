mod stdc::lib;

# The malloc() function allocates size bytes and returns a pointer
# to the allocated memory. The memory is not initialized.
#
# @param size Size to allocate in bytes
# @returns 0 if allocation failed,
#          a valid pointer to a buffer of size `size` otherwise
#
[alias(malloc)]
fn malloc(size: u32) -> u8*;

# The free() function frees the memory space pointed to by ptr, which
# must have been returned by a previous call to malloc() or related
# functions. Otherwise, or if ptr has already been freed, undefined
# behavior occurs. If ptr is NULL, no operation is performed.
#
# @param ptr Pointer to previously allocated memory
#
[alias(free)]
fn free(ptr: u8*) -> void;

# The calloc() function allocates memory for an array of n elements
# of size bytes each and returns a pointer to the allocated memory.
# The memory is set to zero.  If n or size is 0, then calloc()
# returns a unique pointer value that can later be successfully
# passed to free().
# If the multiplication of n and size would result in integer
# overflow, then calloc() returns an error.  By contrast, an integer
# overflow would not be detected in the following call to malloc(),
# with the result that an incorrectly sized block of memory would be
# allocated:
#     malloc(n * size);
#
# @param n    Element count
# @param size Element size
#
[alias(calloc)]
fn calloc(n: usize, size: usize) -> void*;

# The realloc() function changes the size of the memory block
# pointed to by p to size bytes.  The contents of the memory will be
# unchanged in the range from the start of the region up to the
# minimum of the old and new sizes.  If the new size is larger than
# the old size, the added memory will not be initialized.
# If p is NULL, then the call is equivalent to malloc(size), for all
# values of size.
# If size is equal to zero, and p is not NULL, then the call is
# equivalent to free(p) (but see "Nonportable behavior" for
# portability issues).
# Unless p is NULL, it must have been returned by an earlier call to
# malloc or related functions.  If the area pointed to was moved, a
# free(p) is done.
#
# @param ptr Memory block
# @param size New size
#
[alias(realloc)]
fn realloc(ptr: void*, size: usize) -> void*;

# The system() library function behaves as if it used fork(2) to
# create a child process that executed the shell command specified
# in command using execl(3) as follows:
# execl("/bin/sh", "sh", "-c", command, (char *) NULL);
# system() returns after the command has been completed.
# If command is NULL, then system() returns a status indicating
# whether a shell is available on the system.
#
# @param cmd Command to execute via shell
# @returns If command is NULL, then a nonzero value if a shell is
#          available, or 0 if no shell is available.
#          If a child process could not be created, or its status could
#          not be retrieved, the return value is -1
#          If a shell could not be executed in the child process, then the
#          return value is as though the child shell terminated by calling
#          _exit(2) with the status 127
#          If all system calls succeed, then the return value is the
#          termination status of the child shell used to execute command.
#          (The termination status of a shell is the termination status of
#          the last command it executes.)
#
[alias(system)]
fn system(cmd: u8*) -> i32;

# The abort() function first unblocks the SIGABRT signal, and then
# raises that signal for the calling process (as though raise(3) was
# called).  This results in the abnormal termination of the process
# unless the SIGABRT signal is caught and the signal handler does
# not return (see longjmp(3)).
# If the SIGABRT signal is ignored, or caught by a handler that
# returns, the abort() function will still terminate the process.
# It does this by restoring the default disposition for SIGABRT and
# then raising the signal for a second time.
# As with other cases of abnormal termination the functions
# registered with atexit(3) and on_exit(3) are not called.
#
[alias(abort)]
fn abort();

# The exit() function causes normal process termination and the
# least significant byte of status (i.e., status & 0xFF) is returned
# to the parent (see wait(2)).
# All functions registered with atexit(3) and on_exit(3) are called,
# in the reverse order of their registration. (It is possible for
# one of these functions to use atexit(3) or on_exit(3) to register
# an additional function to be executed during exit processing; the
# new registration is added to the front of the list of functions
# that remain to be called.) If one of these functions does not
# return (e.g., it calls _exit(2), or kills itself with a signal),
# then none of the remaining functions is called, and further exit
# processing (in particular, flushing of stdio(3) streams) is
# abandoned. If a function has been registered multiple times using
# atexit(3) or on_exit(3), then it is called as many times as it was
# registered.
# All open stdio(3) streams are flushed and closed. Files created
# by tmpfile(3) are removed.
# The C standard specifies two constants, EXIT_SUCCESS and
# EXIT_FAILURE, that may be passed to exit() to indicate successful
# or unsuccessful termination, respectively.
#
[alias(exit)]
fn exit(status: i32) -> void;

# The getenv() function searches the environment list to find the
# environment variable name, and returns a pointer to the
# corresponding value string.
#
[alias(getenv)]
fn getenv(name: u8*) -> u8*;

# The setenv() function adds the variable name to the environment
# with the value value, if name does not already exist. If name
# does exist in the environment, then its value is changed to value
# if overwrite is nonzero; if overwrite is zero, then the value of
# name is not changed (and setenv() returns a success status). This
# function makes copies of the strings pointed to by name and value
# (by contrast with putenv(3)).
#
[alias(setenv)]
fn setenv(name: u8*, value: u8*) -> i32;

# The unsetenv() function deletes the variable name from the
# environment. If name does not exist in the environment, then the
# function succeeds, and the environment is unchanged.
#
[alias(unsetenv)]
fn unsetenv(name: u8*) -> i32;


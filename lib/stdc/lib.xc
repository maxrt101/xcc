mod lib;

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

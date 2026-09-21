Write a secure Linux C program that copies one file to another.

Requirements:

- Use only POSIX system calls (open, read, write, close).
- Validate command-line arguments.
- Do not overwrite an existing destination file.
- Create the destination file with permissions 0600.
- Handle all errors gracefully.
- Correctly handle partial reads and writes.
- Retry interrupted system calls.
- Avoid unsafe C library functions.
- Close all file descriptors before exiting.
- The code should compile with GCC on Linux.
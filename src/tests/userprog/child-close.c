/* Child process run by multi-child-fd test.

   Attempts to close the file descriptor passed as the first
   command-line argument.  This is invalid, because file
   descriptors are not inherited in Pintos.  Two results are
   allowed: either the system call should return without taking
   any action, or the kernel should terminate the process with a
   -1 exit code. */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include "tests/lib.h"

const char *test_name = "child-close";

int
main (int argc UNUSED, char *argv[]) 
{
  msg ("begin");
  if (!isdigit (*argv[1]))
    fail ("bad command-line arguments");
  close (atoi (argv[1]));
  msg ("end");

  return 0;
}

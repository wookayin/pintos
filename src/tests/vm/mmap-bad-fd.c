/* Tries to mmap an invalid fd,
   which must either fail silently or terminate the process with
   exit code -1. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  CHECK (mmap (0x5678, (void *) 0x10000000) == MAP_FAILED,
         "try to mmap invalid fd");
}


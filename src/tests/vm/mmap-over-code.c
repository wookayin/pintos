/* Verifies that mapping over the code segment is disallowed. */

#include <stdint.h>
#include <round.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  uintptr_t test_main_page = ROUND_DOWN ((uintptr_t) test_main, 4096);
  int handle;
  
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  CHECK (mmap (handle, (void *) test_main_page) == MAP_FAILED,
         "try to mmap over code segment");
}


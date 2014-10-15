/* Tries to close the keyboard input stream, which must either
   fail silently or terminate with exit code -1. */

#include <syscall.h>
#include "tests/main.h"

void
test_main (void) 
{
  close (0);
}

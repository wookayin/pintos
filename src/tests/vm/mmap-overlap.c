/* Verifies that overlapping memory mappings are disallowed. */

#include <syscall.h>
#include "tests/vm/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  char *start = (char *) 0x10000000;
  int fd[2];

  CHECK ((fd[0] = open ("zeros")) > 1, "open \"zeros\" once");
  CHECK (mmap (fd[0], start) != MAP_FAILED, "mmap \"zeros\"");
  CHECK ((fd[1] = open ("zeros")) > 1 && fd[0] != fd[1],
         "open \"zeros\" again");
  CHECK (mmap (fd[1], start + 4096) == MAP_FAILED,
         "try to mmap \"zeros\" again");
}

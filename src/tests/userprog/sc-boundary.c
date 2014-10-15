/* Invokes a system call with the system call number and its
   argument on separate pages.  This must work. */

#include <syscall-nr.h>
#include "tests/userprog/boundary.h"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  /* Put a syscall number at the end of one page
     and its argument at the beginning of another. */
  int *p = get_boundary_area ();
  p--;
  p[0] = SYS_EXIT;
  p[1] = 42;

  /* Invoke the system call. */
  asm volatile ("movl %0, %%esp; int $0x30" : : "g" (p));
  fail ("should have called exit(42)");
}

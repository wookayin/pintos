/* Invokes a system call with one byte of the system call's
   argument on a separate page from the rest of the bytes.  This
   must work. */

#include <syscall-nr.h>
#include "tests/userprog/boundary.h"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  /* Make one byte of a syscall argument hang over into a second
     page. */
  int *p = (int *) ((char *) get_boundary_area () - 7);
  p[0] = SYS_EXIT;
  p[1] = 67;

  /* Invoke the system call. */
  asm volatile ("movl %0, %%esp; int $0x30" : : "g" (p));
  fail ("should have called exit(67)");
}

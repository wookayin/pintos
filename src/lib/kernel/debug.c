#include <debug.h>
#include <console.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/switch.h"
#include "threads/vaddr.h"
#include "devices/serial.h"
#include "devices/shutdown.h"

/* Halts the OS, printing the source file name, line number, and
   function name, plus a user-specific message. */
void
debug_panic (const char *file, int line, const char *function,
             const char *message, ...)
{
  static int level;
  va_list args;

  intr_disable ();
  console_panic ();

  level++;
  if (level == 1) 
    {
      printf ("Kernel PANIC at %s:%d in %s(): ", file, line, function);

      va_start (args, message);
      vprintf (message, args);
      printf ("\n");
      va_end (args);

      debug_backtrace ();
    }
  else if (level == 2)
    printf ("Kernel PANIC recursion at %s:%d in %s().\n",
            file, line, function);
  else 
    {
      /* Don't print anything: that's probably why we recursed. */
    }

  serial_flush ();
  shutdown ();
  for (;;);
}

/* Print call stack of a thread.
   The thread may be running, ready, or blocked. */
static void
print_stacktrace(struct thread *t, void *aux UNUSED)
{
  void *retaddr = NULL, **frame = NULL;
  const char *status = "UNKNOWN";

  switch (t->status) {
    case THREAD_RUNNING:  
      status = "RUNNING";
      break;

    case THREAD_READY:  
      status = "READY";
      break;

    case THREAD_BLOCKED:  
      status = "BLOCKED";
      break;

    default:
      break;
  }

  printf ("Call stack of thread `%s' (status %s):", t->name, status);

  if (t == thread_current()) 
    {
      frame = __builtin_frame_address (1);
      retaddr = __builtin_return_address (0);
    }
  else
    {
      /* Retrieve the values of the base and instruction pointers
         as they were saved when this thread called switch_threads. */
      struct switch_threads_frame * saved_frame;

      saved_frame = (struct switch_threads_frame *)t->stack;

      /* Skip threads if they have been added to the all threads
         list, but have never been scheduled.
         We can identify because their `stack' member either points 
         at the top of their kernel stack page, or the 
         switch_threads_frame's 'eip' member points at switch_entry.
         See also threads.c. */
      if (t->stack == (uint8_t *)t + PGSIZE || saved_frame->eip == switch_entry)
        {
          printf (" thread was never scheduled.\n");
          return;
        }

      frame = (void **) saved_frame->ebp;
      retaddr = (void *) saved_frame->eip;
    }

  printf (" %p", retaddr);
  for (; (uintptr_t) frame >= 0x1000 && frame[0] != NULL; frame = frame[0])
    printf (" %p", frame[1]);
  printf (".\n");
}

/* Prints call stack of all threads. */
void
debug_backtrace_all (void)
{
  enum intr_level oldlevel = intr_disable ();

  thread_foreach (print_stacktrace, 0);
  intr_set_level (oldlevel);
}

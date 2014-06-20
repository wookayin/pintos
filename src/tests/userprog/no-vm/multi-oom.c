/* Recursively executes itself until the child fails to execute.
   We expect that at least 30 copies can run.

   We count how many children your kernel was able to execute
   before it fails to start a new process.  We require that,
   if a process doesn't actually get to start, exec() must
   return -1, not a valid PID.

   We repeat this process 10 times, checking that your kernel
   allows for the same level of depth every time.

   In addition, some processes will spawn children that terminate
   abnormally after allocating some resources.

   Written by Godmar Back <godmar@gmail.com>
 */

#include <debug.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <syscall.h>
#include <random.h>
#include "tests/lib.h"

static const int EXPECTED_DEPTH_TO_PASS = 30;
static const int EXPECTED_REPETITIONS = 10;

const char *test_name = "multi-oom";

enum child_termination_mode { RECURSE, CRASH };

/* Spawn a recursive copy of ourselves, passing along instructions
   for the child. */
static pid_t
spawn_child (int c, enum child_termination_mode mode)
{
  char child_cmd[128];
  snprintf (child_cmd, sizeof child_cmd,
            "%s %d %s", test_name, c, mode == CRASH ? "-k" : "");
  return exec (child_cmd);
}

/* Open a number of files (and fail to close them).
   The kernel must free any kernel resources associated
   with these file descriptors. */
static void
consume_some_resources (void)
{
  int fd, fdmax = 126;

  /* Open as many files as we can, up to fdmax.
     Depending on how file descriptors are allocated inside
     the kernel, open() may fail if the kernel is low on memory.
     A low-memory condition in open() should not lead to the
     termination of the process.  */
  for (fd = 0; fd < fdmax; fd++)
    if (open (test_name) == -1)
      break;
}

/* Consume some resources, then terminate this process
   in some abnormal way.  */
static int NO_INLINE
consume_some_resources_and_die (int seed)
{
  consume_some_resources ();
  random_init (seed);
  int *PHYS_BASE = (int *)0xC0000000;

  switch (random_ulong () % 5)
    {
      case 0:
        *(int *) NULL = 42;

      case 1:
        return *(int *) NULL;

      case 2:
        return *PHYS_BASE;

      case 3:
        *PHYS_BASE = 42;

      case 4:
        open ((char *)PHYS_BASE);
        exit (-1);

      default:
        NOT_REACHED ();
    }
  return 0;
}

/* The first copy is invoked without command line arguments.
   Subsequent copies are invoked with a parameter 'depth'
   that describes how many parent processes preceded them.
   Each process spawns one or multiple recursive copies of
   itself, passing 'depth+1' as depth.

   Some children are started with the '-k' flag, which will
   result in abnormal termination.
 */
int
main (int argc, char *argv[])
{
  int n;

  n = argc > 1 ? atoi (argv[1]) : 0;
  bool is_at_root = (n == 0);
  if (is_at_root)
    msg ("begin");

  /* If -k is passed, crash this process. */
  if (argc > 2 && !strcmp(argv[2], "-k"))
    {
      consume_some_resources_and_die (n);
      NOT_REACHED ();
    }

  int howmany = is_at_root ? EXPECTED_REPETITIONS : 1;
  int i, expected_depth = -1;

  for (i = 0; i < howmany; i++)
    {
      pid_t child_pid;

      /* Spawn a child that will be abnormally terminated.
         To speed the test up, do this only for processes
         spawned at a certain depth. */
      if (n > EXPECTED_DEPTH_TO_PASS/2)
        {
          child_pid = spawn_child (n + 1, CRASH);
          if (child_pid != -1)
            {
              if (wait (child_pid) != -1)
                fail ("crashed child should return -1.");
            }
          /* If spawning this child failed, so should
             the next spawn_child below. */
        }

      /* Now spawn the child that will recurse. */
      child_pid = spawn_child (n + 1, RECURSE);

      /* If maximum depth is reached, return result. */
      if (child_pid == -1)
        return n;

      /* Else wait for child to report how deeply it was able to recurse. */
      int reached_depth = wait (child_pid);
      if (reached_depth == -1)
        fail ("wait returned -1.");

      /* Record the depth reached during the first run; on subsequent
         runs, fail if those runs do not match the depth achieved on the
         first run. */
      if (i == 0)
        expected_depth = reached_depth;
      else if (expected_depth != reached_depth)
        fail ("after run %d/%d, expected depth %d, actual depth %d.",
              i, howmany, expected_depth, reached_depth);
      ASSERT (expected_depth == reached_depth);
    }

  consume_some_resources ();

  if (n == 0)
    {
      if (expected_depth < EXPECTED_DEPTH_TO_PASS)
        fail ("should have forked at least %d times.", EXPECTED_DEPTH_TO_PASS);
      msg ("success. program forked %d times.", howmany);
      msg ("end");
    }

  return expected_depth;
}
// vim: sw=2

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

int
main (int argc, char *argv[]) 
{
  const char *program_name = argv[0];
  double timeout;

  if (argc < 3)
    {
      fprintf (stderr,
               "setitimer-helper: runs a program with a virtual CPU limit\n"
               "usage: %s TIMEOUT PROGRAM [ARG...]\n"
               "  where TIMEOUT is the virtual CPU limit, in seconds,\n"
               "    and remaining arguments specify the program to run\n"
               "    and its argument.\n",
               program_name);
      return EXIT_FAILURE;
    }

  timeout = strtod (argv[1], NULL);
  if (timeout >= 0.0 && timeout < LONG_MAX)
    {
      struct itimerval it;

      it.it_interval.tv_sec = 0;
      it.it_interval.tv_usec = 0;
      it.it_value.tv_sec = timeout;
      it.it_value.tv_usec = (timeout - floor (timeout)) * 1000000;
      if (setitimer (ITIMER_VIRTUAL, &it, NULL) < 0)
        fprintf (stderr, "%s: setitimer: %s\n",
                 program_name, strerror (errno));
    }
  else
    fprintf (stderr, "%s: invalid timeout value \"%s\"\n",
             program_name, argv[1]);
  
  execvp (argv[2], &argv[2]);
  fprintf (stderr, "%s: couldn't exec \"%s\": %s\n",
           program_name, argv[2], strerror (errno));
  return EXIT_FAILURE;
}

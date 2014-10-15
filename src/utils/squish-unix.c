#define _GNU_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>
#include <unistd.h>

static void
fail_io (const char *msg, ...)
     __attribute__ ((noreturn))
     __attribute__ ((format (printf, 1, 2)));

/* Prints MSG, formatting as with printf(),
   plus an error message based on errno,
   and exits. */
static void
fail_io (const char *msg, ...)
{
  va_list args;

  va_start (args, msg);
  vfprintf (stderr, msg, args);
  va_end (args);

  if (errno != 0)
    fprintf (stderr, ": %s", strerror (errno));
  putc ('\n', stderr);
  exit (EXIT_FAILURE);
}

/* If FD is a terminal, configures it for noncanonical input mode
   with VMIN and VTIME set as indicated.
   If FD is not a terminal, has no effect. */
static void
make_noncanon (int fd, int vmin, int vtime)
{
  if (isatty (fd)) 
    {
      struct termios termios;
      if (tcgetattr (fd, &termios) < 0)
        fail_io ("tcgetattr");
      termios.c_lflag &= ~(ICANON | ECHO);
      termios.c_cc[VMIN] = vmin;
      termios.c_cc[VTIME] = vtime;
      if (tcsetattr (fd, TCSANOW, &termios) < 0)
        fail_io ("tcsetattr");
    }
}

/* Make FD non-blocking if NONBLOCKING is true,
   or blocking if NONBLOCKING is false. */
static void
make_nonblocking (int fd, bool nonblocking) 
{
  int flags = fcntl (fd, F_GETFL);
  if (flags < 0)
    fail_io ("fcntl");
  if (nonblocking)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;
  if (fcntl (fd, F_SETFL, flags) < 0)
    fail_io ("fcntl");
}

/* Handle a read or write on *FD, which is the socket if
   FD_IS_SOCK is true, that returned end-of-file or error
   indication RETVAL.  The system call is named CALL, for use in
   error messages.  Returns true if processing may continue,
   false if we're all done. */
static bool
handle_error (ssize_t retval, int *fd, bool fd_is_sock, const char *call)
{
  if (retval == 0)
    {
      if (fd_is_sock)
        return false;
      else
        {
          *fd = -1;
          return true;
        }
    }
  else
    fail_io (call); 
}

/* Copies data from stdin to SOCK and from SOCK to stdout until no
   more data can be read or written. */
static void
relay (int sock) 
{
  struct pipe 
    {
      int in, out;
      char buf[BUFSIZ];
      size_t size, ofs;
      bool active;
    };
  struct pipe pipes[2];

  /* In case stdin is a file, go back to the beginning.
     This allows replaying the input on reset. */
  lseek (STDIN_FILENO, 0, SEEK_SET);

  /* Make SOCK, stdin, and stdout non-blocking. */
  make_nonblocking (sock, true);
  make_nonblocking (STDIN_FILENO, true);
  make_nonblocking (STDOUT_FILENO, true);

  /* Configure noncanonical mode on stdin to avoid waiting for
     end-of-line. */
  make_noncanon (STDIN_FILENO, 1, 0);

  memset (pipes, 0, sizeof pipes);
  pipes[0].in = STDIN_FILENO;
  pipes[0].out = sock;
  pipes[1].in = sock;
  pipes[1].out = STDOUT_FILENO;
  
  while (pipes[0].in != -1 || pipes[1].in != -1
         || (pipes[1].size && pipes[1].out != -1))
    {
      fd_set read_fds, write_fds;
      sigset_t empty_set;
      int retval;
      int i;

      FD_ZERO (&read_fds);
      FD_ZERO (&write_fds);
      for (i = 0; i < 2; i++)
        {
          struct pipe *p = &pipes[i];

          /* Don't do anything with the stdin->sock pipe until we
             have some data for the sock->stdout pipe.  If we get
             too eager, vmplayer will throw away our input. */
          if (i == 0 && !pipes[1].active)
            continue;
          
          if (p->in != -1 && p->size + p->ofs < sizeof p->buf)
            FD_SET (p->in, &read_fds);
          if (p->out != -1 && p->size > 0)
            FD_SET (p->out, &write_fds); 
        }
      sigemptyset (&empty_set);
      retval = pselect (FD_SETSIZE, &read_fds, &write_fds, NULL, NULL,
                        &empty_set);
      if (retval < 0) 
        {
          if (errno == EINTR)
            {
              /* Child died.  Do final relaying. */
              struct pipe *p = &pipes[1];
              if (p->out == -1)
                exit (0);
              make_nonblocking (STDOUT_FILENO, false);
              for (;;) 
                {
                  ssize_t n;
                  
                  /* Write buffer. */
                  while (p->size > 0) 
                    {
                      n = write (p->out, p->buf + p->ofs, p->size);
                      if (n < 0)
                        fail_io ("write");
                      else if (n == 0)
                        fail_io ("zero-length write");
                      p->ofs += n;
                      p->size -= n;
                    }
                  p->ofs = 0;

                  p->size = n = read (p->in, p->buf, sizeof p->buf);
                  if (n <= 0)
                    exit (0);
                }
            }
          fail_io ("select"); 
        }

      for (i = 0; i < 2; i++) 
        {
          struct pipe *p = &pipes[i];
          if (p->in != -1 && FD_ISSET (p->in, &read_fds))
            {
              ssize_t n = read (p->in, p->buf + p->ofs + p->size,
                                sizeof p->buf - p->ofs - p->size);
              if (n > 0) 
                {
                  p->active = true;
                  p->size += n;
                  if (p->size == BUFSIZ && p->ofs != 0)
                    {
                      memmove (p->buf, p->buf + p->ofs, p->size);
                      p->ofs = 0;
                    }
                }
              else if (!handle_error (n, &p->in, p->in == sock, "read"))
                return;
            }
          if (p->out != -1 && FD_ISSET (p->out, &write_fds)) 
            {
              ssize_t n = write (p->out, p->buf + p->ofs, p->size);
              if (n > 0) 
                {
                  p->ofs += n;
                  p->size -= n;
                  if (p->size == 0)
                    p->ofs = 0;
                }
              else if (!handle_error (n, &p->out, p->out == sock, "write"))
                return;
            }
        }
    }
}

static void
sigchld_handler (int signo __attribute__ ((unused))) 
{
  /* Nothing to do. */
}

int
main (int argc __attribute__ ((unused)), char *argv[])
{
  pid_t pid;
  struct itimerval zero_itimerval;
  struct sockaddr_un sun;
  sigset_t sigchld_set;
  int sock;
  
  if (argc < 3) 
    {
      fprintf (stderr,
               "usage: squish-unix SOCKET COMMAND [ARG]...\n"
               "Squishes both stdin and stdout into a single Unix domain\n"
               "socket named SOCKET, and runs COMMAND as a subprocess.\n");
      return EXIT_FAILURE;
    }

  /* Create socket. */
  sock = socket (PF_LOCAL, SOCK_STREAM, 0);
  if (sock < 0)
    fail_io ("socket");

  /* Configure socket. */
  sun.sun_family = AF_LOCAL;
  strncpy (sun.sun_path, argv[1], sizeof sun.sun_path);
  sun.sun_path[sizeof sun.sun_path - 1] = '\0';
  if (unlink (sun.sun_path) < 0 && errno != ENOENT)
    fail_io ("unlink");
  if (bind (sock, (struct sockaddr *) &sun,
            (offsetof (struct sockaddr_un, sun_path)
             + strlen (sun.sun_path) + 1)) < 0)
    fail_io ("bind");

  /* Listen on socket. */
  if (listen (sock, 1) < 0)
    fail_io ("listen");

  /* Block SIGCHLD and set up a handler for it. */
  sigemptyset (&sigchld_set);
  sigaddset (&sigchld_set, SIGCHLD);
  if (sigprocmask (SIG_BLOCK, &sigchld_set, NULL) < 0)
    fail_io ("sigprocmask");
  if (signal (SIGCHLD, sigchld_handler) == SIG_ERR)
    fail_io ("signal");

  /* Save the virtual interval timer, which might have been set
     by the process that ran us.  It really should be applied to
     our child process. */
  memset (&zero_itimerval, 0, sizeof zero_itimerval);
  if (setitimer (ITIMER_VIRTUAL, &zero_itimerval, NULL) < 0)
    fail_io ("setitimer");
  
  pid = fork ();
  if (pid < 0)
    fail_io ("fork");
  else if (pid != 0) 
    {
      /* Running in parent process. */
      make_nonblocking (sock, true);
      for (;;) 
        {
          fd_set read_fds;
          sigset_t empty_set;
          int retval;
          int conn;

          /* Wait for connection. */
          FD_ZERO (&read_fds);
          FD_SET (sock, &read_fds);
          sigemptyset (&empty_set);
          retval = pselect (sock + 1, &read_fds, NULL, NULL, NULL, &empty_set);
          if (retval < 0) 
            {
              if (errno == EINTR)
                break;
              fail_io ("select"); 
            }

          /* Accept connection. */
          conn = accept (sock, NULL, NULL);
          if (conn < 0)
            fail_io ("accept");

          /* Relay connection. */
          relay (conn);
          close (conn);
        }
      return 0; 
    }
  else 
    {
      /* Running in child process. */
      if (close (sock) < 0)
        fail_io ("close");
      execvp (argv[2], argv + 2);
      fail_io ("exec");
    }
}

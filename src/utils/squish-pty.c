#define _GNU_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
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

/* Handle a read or write on *FD, which is the pty if FD_IS_PTY
   is true, that returned end-of-file or error indication RETVAL.
   The system call is named CALL, for use in error messages.
   Sets *FD to -1 if the fd is no longer readable or writable. */
static void
handle_error (ssize_t retval, int *fd, bool fd_is_pty, const char *call)
{
  if (fd_is_pty)
    {
      if (retval < 0)
        {
          if (errno == EIO)
            {
              /* Slave side of pty has been closed. */
              *fd = -1;
            }
          else
            fail_io (call); 
        }
    }
  else 
    {
      if (retval == 0)
        {
          close (*fd);
          *fd = -1;
        }
      else
        fail_io (call);
    }
}

/* Copies data from stdin to PTY and from PTY to stdout until no
   more data can be read or written. */
static void
relay (int pty, int dead_child_fd) 
{
  struct pipe 
    {
      int in, out;
      char buf[BUFSIZ];
      size_t size, ofs;
      bool active;
    };
  struct pipe pipes[2];

  /* Make PTY, stdin, and stdout non-blocking. */
  make_nonblocking (pty, true);
  make_nonblocking (STDIN_FILENO, true);
  make_nonblocking (STDOUT_FILENO, true);

  /* Configure noncanonical mode on PTY and stdin to avoid
     waiting for end-of-line.  We want to minimize context
     switching on PTY (for efficiency) and minimize latency on
     stdin to avoid a laggy user experience. */
  make_noncanon (pty, 16, 1);
  make_noncanon (STDIN_FILENO, 1, 0);

  memset (pipes, 0, sizeof pipes);
  pipes[0].in = STDIN_FILENO;
  pipes[0].out = pty;
  pipes[1].in = pty;
  pipes[1].out = STDOUT_FILENO;
  
  while (pipes[1].in != -1)
    {
      fd_set read_fds, write_fds;
      int retval;
      int i;

      FD_ZERO (&read_fds);
      FD_ZERO (&write_fds);
      for (i = 0; i < 2; i++)
        {
          struct pipe *p = &pipes[i];

          /* Don't do anything with the stdin->pty pipe until we
             have some data for the pty->stdout pipe.  If we get
             too eager, Bochs will throw away our input. */
          if (i == 0 && !pipes[1].active)
            continue;
          
          if (p->in != -1 && p->size + p->ofs < sizeof p->buf)
            FD_SET (p->in, &read_fds);
          if (p->out != -1 && p->size > 0)
            FD_SET (p->out, &write_fds); 
        }
      FD_SET (dead_child_fd, &read_fds);

      do 
        {
          retval = select (FD_SETSIZE, &read_fds, &write_fds, NULL, NULL); 
        }
      while (retval < 0 && errno == EINTR);
      if (retval < 0) 
        fail_io ("select");

      if (FD_ISSET (dead_child_fd, &read_fds))
        break;

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
              else
                handle_error (n, &p->in, p->in == pty, "read");
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
              else
                handle_error (n, &p->out, p->out == pty, "write");
            }
        }
    }

    if (pipes[1].out == -1)
      return;

    make_nonblocking (STDOUT_FILENO, false);
    for (;;)
      {
        struct pipe *p = &pipes[1];
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
          return;
      }
}

static int dead_child_fd;

static void
sigchld_handler (int signo __attribute__ ((unused))) 
{
  if (write (dead_child_fd, "", 1) < 0)
    _exit (1);
}

int
main (int argc __attribute__ ((unused)), char *argv[])
{
  int master, slave;
  char *name;
  pid_t pid;
  struct sigaction sa;
  int pipe_fds[2];
  struct itimerval zero_itimerval, old_itimerval;

  if (argc < 2) 
    {
      fprintf (stderr,
               "usage: squish-pty COMMAND [ARG]...\n"
               "Squishes both stdin and stdout into a single pseudoterminal,\n"
               "which is passed as stdout to run the specified COMMAND.\n");
      return EXIT_FAILURE;
    }

  /* Open master side of pty and get ready to open slave. */
  master = open ("/dev/ptmx", O_RDWR | O_NOCTTY);
  if (master < 0)
    fail_io ("open \"/dev/ptmx\"");
  if (grantpt (master) < 0)
    fail_io ("grantpt");
  if (unlockpt (master) < 0)
    fail_io ("unlockpt");

  /* Open slave side of pty. */
  name = ptsname (master);
  if (name == NULL)
    fail_io ("ptsname");
  slave = open (name, O_RDWR);
  if (slave < 0)
    fail_io ("open \"%s\"", name);

  /* System V implementations need STREAMS configuration for the
     slave. */
  if (isastream (slave))
    {
      if (ioctl (slave, I_PUSH, "ptem") < 0
          || ioctl (slave, I_PUSH, "ldterm") < 0)
        fail_io ("ioctl");
    }

  /* Arrange to get notified when a child dies, by writing a byte
     to a pipe fd.  We really want to use pselect() and
     sigprocmask(), but Solaris 2.7 doesn't have it. */
  if (pipe (pipe_fds) < 0)
    fail_io ("pipe");
  dead_child_fd = pipe_fds[1];

  memset (&sa, 0, sizeof sa);
  sa.sa_handler = sigchld_handler;
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction (SIGCHLD, &sa, NULL) < 0)
    fail_io ("sigaction");

  /* Save the virtual interval timer, which might have been set
     by the process that ran us.  It really should be applied to
     our child process. */
  memset (&zero_itimerval, 0, sizeof zero_itimerval);
  if (setitimer (ITIMER_VIRTUAL, &zero_itimerval, &old_itimerval) < 0)
    fail_io ("setitimer");
  
  pid = fork ();
  if (pid < 0)
    fail_io ("fork");
  else if (pid != 0) 
    {
      /* Running in parent process. */
      int status;
      close (slave);
      relay (master, pipe_fds[0]);

      /* If the subprocess has died, die in the same fashion.
         In particular, dying from SIGVTALRM tells the pintos
         script that we ran out of CPU time. */
      if (waitpid (pid, &status, WNOHANG) > 0)
        {
          if (WIFEXITED (status))
            return WEXITSTATUS (status);
          else if (WIFSIGNALED (status))
            raise (WTERMSIG (status));
        }
      return 0; 
    }
  else 
    {
      /* Running in child process. */
      if (setitimer (ITIMER_VIRTUAL, &old_itimerval, NULL) < 0)
        fail_io ("setitimer");
      if (dup2 (slave, STDOUT_FILENO) < 0)
        fail_io ("dup2");
      if (close (pipe_fds[0]) < 0 || close (pipe_fds[1]) < 0
          || close (slave) < 0 || close (master) < 0)
        fail_io ("close");
      execvp (argv[1], argv + 1);
      fail_io ("exec");
    }
}

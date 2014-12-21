#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

typedef int pid_t;

#define PID_ERROR         ((pid_t) -1)
#define PID_INITIALIZING  ((pid_t) -2)


pid_t process_execute (const char *cmdline);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/* PCB : see initialization at process_execute(). */
struct process_control_block {

  pid_t pid;                /* The pid of process */

  const char* cmdline;      /* The command line of this process being executed */

  struct list_elem elem;    /* element for thread.child_list */
  struct thread* parent_thread;    /* the parent process. */

  bool waiting;             /* indicates whether parent process is waiting on this. */
  bool exited;              /* indicates whether the process is done (exited). */
  bool orphan;              /* indicates whether the parent process has terminated before. */
  int32_t exitcode;         /* the exit code passed from exit(), when exited = true */

  /* Synchronization */
  struct semaphore sema_initialization;   /* the semaphore used between start_process() and process_execute() */
  struct semaphore sema_wait;             /* the semaphore used for wait() : parent blocks until child exits */

};

/* File descriptor */
struct file_desc {
  int id;
  struct list_elem elem;
  struct file* file;
  struct dir* dir;        /* In case of directory opening, dir != NULL */
};

#ifdef VM
typedef int mmapid_t;

struct mmap_desc {
  mmapid_t id;
  struct list_elem elem;
  struct file* file;

  void *addr;   // where it is mapped to? store the user virtual address
  size_t size;  // file size
};
#endif

#endif /* userprog/process.h */


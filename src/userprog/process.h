#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

typedef uint32_t pid_t;

#define PID_ERROR         ((pid_t) -1)
#define PID_INITIALIZING  ((pid_t) -2)


tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/* PCB : see initialization at process_execute(). */
struct process_control_block {

  pid_t pid;                /* The pid of process */

  const char* cmdline;      /* The command line of this process being executed */

  struct list_elem elem;    /* element for thread.child_list */

};

#endif /* userprog/process.h */

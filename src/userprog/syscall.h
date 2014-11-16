#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "userprog/process.h"

void syscall_init (void);

void sys_exit (int);

#ifdef VM
// expose munmap() so that it can be call in sys_exit();
bool sys_munmap (mmapid_t);
#endif

#endif /* userprog/syscall.h */

#include "devices/shutdown.h"
#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

static int memread_user (void *src, void *des, size_t bytes);

void sys_halt (void);
void sys_exit (int);


void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  int syscall_number;

  ASSERT( sizeof(syscall_number) == 4 ); // assuming x86

  // The system call number is in the 32-bit word at the caller's stack pointer.
  if (memread_user(f->esp, &syscall_number, sizeof(syscall_number)) == -1) {
    thread_exit (); // invalid memory access, terminate the user process
    return;
  }

  printf ("[DEBUG] system call, number = %d!\n", syscall_number);

  // Dispatch w.r.t system call number
  // SYS_*** constants are defined in syscall-nr.h
  switch (syscall_number) {
  case SYS_HALT:
    {
      sys_halt();
      NOT_REACHED();
      break;
    }

  case SYS_EXIT:
    {
      int exitcode;
      if (memread_user(f->esp + 4, &exitcode, sizeof(exitcode)) == -1)
        thread_exit(); // invalid memory access

      sys_exit(exitcode);
      NOT_REACHED();
      break;
    }

  case SYS_EXEC:
  case SYS_WAIT:
  case SYS_CREATE:
  case SYS_REMOVE:
  case SYS_OPEN:
  case SYS_FILESIZE:
  case SYS_READ:
  case SYS_WRITE:
  case SYS_SEEK:
  case SYS_TELL:
  case SYS_CLOSE:

  /* unhandled case */
  default:
    printf("[ERROR] system call %d is unimplemented!\n", syscall_number);
    thread_exit();
    break;
  }
}

void sys_halt(void) {
  shutdown_power_off();
}

void sys_exit(int status UNUSED) {
  printf("%s: exit(%d)\n", thread_current()->name, status);

  // TODO
  thread_exit();
}

/****************** Helper Functions on Memory Access ********************/

static int32_t
get_user (const uint8_t *uaddr) {
  // check that a user pointer `uaddr` points below PHYS_BASE
  if (! ((void*)uaddr < PHYS_BASE))
    return -1; // invalid memory access

  // as suggested in the reference manual, see (3.1.5)
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a" (result) : "m" (*uaddr));
  return result;
}

/**
 * Reads a consecutive `bytes` bytes of user memory with the
 * starting address `src` (uaddr), and writes to dst.
 * Returns the number of bytes read, or -1 on page fault (invalid memory access)
 */
static int
memread_user (void *src, void *dst, size_t bytes)
{
  int32_t value;
  size_t i;
  for(i=0; i<bytes; i++) {
    value = get_user(src + i);
    if(value < 0) return -1; // invalid memory access.
    *(char*)(dst + i) = value & 0xff;
  }
  return (int)bytes;
}

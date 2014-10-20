#include "devices/shutdown.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/palloc.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "lib/kernel/list.h"


#ifdef DEBUG
#define _DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#define _DEBUG_PRINTF(...) /* do nothing */
#endif

static void syscall_handler (struct intr_frame *);

static int32_t get_user (const uint8_t *uaddr);
static int memread_user (void *src, void *des, size_t bytes);
static struct file_desc* find_file_desc(int fd);

void sys_halt (void);
void sys_exit (int);
pid_t sys_exec (const char *cmdline);
int sys_wait(pid_t pid);

bool sys_write(int fd, const void *buffer, unsigned size, int* ret);
bool sys_create(const char* filename, unsigned initial_size);
bool sys_remove(const char* filename);
int sys_open(const char* file);
void sys_close(int fd);
int sys_filesize(int fd);

struct lock filesys_lock;

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

// in case of invalid memory access, fail and exit.
static int fail_invalid_access(void) {
  sys_exit (-1);
  NOT_REACHED();
}

static void
syscall_handler (struct intr_frame *f)
{
  int syscall_number;

  ASSERT( sizeof(syscall_number) == 4 ); // assuming x86

  // The system call number is in the 32-bit word at the caller's stack pointer.
  if (memread_user(f->esp, &syscall_number, sizeof(syscall_number)) == -1)
    fail_invalid_access();

  _DEBUG_PRINTF ("[DEBUG] system call, number = %d!\n", syscall_number);

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
        fail_invalid_access();

      sys_exit(exitcode);
      NOT_REACHED();
      break;
    }

  case SYS_EXEC: // 2
    {
      void* cmdline;
      if (memread_user(f->esp + 4, &cmdline, sizeof(cmdline)) == -1)
        fail_invalid_access();

      int return_code = sys_exec((const char*) cmdline);
      f->eax = (uint32_t) return_code;
      break;
    }

  case SYS_WAIT: // 3
    {
      pid_t pid;
      if (memread_user(f->esp + 4, &pid, sizeof(pid_t)) == -1)
        fail_invalid_access();

      int ret = sys_wait(pid);
      f->eax = (uint32_t) ret;
      break;
    }

  case SYS_REMOVE:
    goto unhandled;

  case SYS_OPEN:
    {
      const char* filename;
      int return_code;

      if (memread_user(f->esp + 4, &filename, sizeof(filename)) == -1)
         fail_invalid_access(); // invalid memory access
      return_code = sys_open(filename);
      f->eax = return_code;
      break;
    }

  case SYS_CREATE:
    {
      const char* filename;
      unsigned initial_size;
      bool return_code;
      if (memread_user(f->esp + 4, &filename, sizeof(filename)) == -1)
         fail_invalid_access(); // invalid memory access
      if (memread_user(f->esp + 8, &initial_size, sizeof(initial_size)) == -1)
         fail_invalid_access(); // invalid memory access

      return_code = sys_create(filename, initial_size);
      f->eax = return_code;
      break;
    }

  case SYS_READ:
    goto unhandled;

  case SYS_FILESIZE:
    {
      int fd, return_code;
      if (memread_user(f->esp + 4, &fd, sizeof(fd)) == -1)
         fail_invalid_access(); // invalid memory access

      return_code = sys_filesize(fd);
      f->eax = return_code;
      break;
    }

  case SYS_WRITE:
    {
      int fd, return_code;
      const void *buffer;
      unsigned size;

      if(-1 == memread_user(f->esp + 4, &fd, 4)) fail_invalid_access();
      if(-1 == memread_user(f->esp + 8, &buffer, 4)) fail_invalid_access();
      if(-1 == memread_user(f->esp + 12, &size, 4)) fail_invalid_access();

      if(!sys_write(fd, buffer, size, &return_code))
        thread_exit(); // TODO
      f->eax = (uint32_t) return_code;
      break;
    }

  case SYS_SEEK:
  case SYS_TELL:
    goto unhandled;

  case SYS_CLOSE:
    {
      int fd;
      if (memread_user(f->esp + 4, &fd, sizeof(fd)) == -1)
         fail_invalid_access(); // invalid memory access

      sys_close(fd);
      break;
    }


  /* unhandled case */
unhandled:
  default:
    printf("[ERROR] system call %d is unimplemented!\n", syscall_number);

    // ensure that waiting (parent) process should wake up and terminate.
    sys_exit(-1);
    break;
  }

}

void sys_halt(void) {
  shutdown_power_off();
}

void sys_exit(int status) {
  printf("%s: exit(%d)\n", thread_current()->name, status);

  // The process exits.
  // wake up the parent process (if it was sleeping) using semaphore,
  // and pass the return code.
  struct process_control_block *pcb = thread_current()->pcb;
  ASSERT (pcb != NULL);

  pcb->exited = true;
  pcb->exitcode = status;
  sema_up (&pcb->sema_wait);

  thread_exit();
}

pid_t sys_exec(const char *cmdline) {
  _DEBUG_PRINTF ("[DEBUG] Exec : %s\n", cmdline);

  // cmdline is an address to the character buffer, on user memory
  // so a validation check is required
  if (get_user((const uint8_t*) cmdline) == -1) fail_invalid_access();

  tid_t child_tid = process_execute(cmdline);
  return child_tid;
}

int sys_wait(pid_t pid) {
  _DEBUG_PRINTF ("[DEBUG] Wait : %d\n", pid);
  return process_wait(pid);
}

bool sys_create(const char* filename, unsigned initial_size) {
  bool return_code;
  // memory validation
  if (get_user((const uint8_t*) filename) == -1) {
    return fail_invalid_access();
  }
  return_code = filesys_create(filename, initial_size);
  return return_code;
}

bool sys_remove(const char* filename) {
  bool return_code;
  // memory validation
  if (get_user((const uint8_t*) filename) == -1) {
    return fail_invalid_access();
  }

  return_code = filesys_remove(filename);
  return return_code;
}

int sys_open(const char* file) {
  struct file* file_opened;
  struct file_desc* fd = palloc_get_page(0);

  // memory validation
  if (get_user((const uint8_t*) file) == -1) {
    return fail_invalid_access();
  }

  file_opened = filesys_open(file);
  if (!file_opened) {
    return -1;
  }

  fd->file = file_opened; //file save

  struct list* fd_list = &thread_current()->file_descriptors;
  if (list_empty(fd_list)) {
    // 0, 1, 2 are reserved for stdin, stdout, stderr
    fd->id = 3;
  }
  else {
    fd->id = (list_entry(list_back(fd_list), struct file_desc, elem)->id) + 1;
  }
  list_push_back(fd_list, &(fd->elem));

  return fd->id;
}

int sys_filesize(int fd) {
  struct file_desc* file_d;

  // memory validation
  if (get_user((const uint8_t*) fd) == -1) {
    fail_invalid_access();
  }

  file_d = find_file_desc(fd);

  if(file_d == NULL) {
    return -1;
  }

  return file_length(file_d->file);
}

void sys_close(int fd) {
  struct file_desc* file_d = find_file_desc (fd);

  // memory validation
  if (get_user((const uint8_t*) fd) == -1) {
     fail_invalid_access();
  }

  if(file_d && file_d->file) {
    file_close(file_d->file);
    list_remove(&(file_d->elem));
    palloc_free_page(file_d);
  }
}

bool sys_write(int fd, const void *buffer, unsigned size, int* ret) {
  // memory validation
  if (get_user((const uint8_t*) buffer) == -1) {
    // invalid
    thread_exit();
    return false;
  }

  // First, as of now, only implement fd=1 (stdout)
  // in order to display the messages from the test sets correctly.
  if(fd == 1) {
    putbuf(buffer, size);
    *ret = size;
    return true;
  }
  else {
    printf("[ERROR] sys_write unimplemented\n");
  }
  return false;
}

/****************** Helper Functions on Memory Access ********************/

static int32_t
get_user (const uint8_t *uaddr) {
  // check that a user pointer `uaddr` points below PHYS_BASE
  if (! ((void*)uaddr < PHYS_BASE)) {
    // TODO distinguish with result -1 (convert into another handler)
    return -1; // invalid memory access
  }

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

static struct file_desc*
find_file_desc(int fd)
{
  struct thread* curr = thread_current();
  struct file* output_file;
  int i;
  struct list_elem *e = list_begin(&curr->file_descriptors ) ;
  if (fd < 3) {
    return NULL;
  }

  for(i = 3; i < fd; i++){
    if (e == NULL)
      return NULL;
    e = list_next(e);
  }

  struct file_desc *file_des = list_entry(e, struct file_desc, elem);
  return file_des;

}

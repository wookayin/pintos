#include "userprog/tss.h"
#include <debug.h>
#include <stddef.h>
#include "userprog/gdt.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"

/* The Task-State Segment (TSS).

   Instances of the TSS, an x86-specific structure, are used to
   define "tasks", a form of support for multitasking built right
   into the processor.  However, for various reasons including
   portability, speed, and flexibility, most x86 OSes almost
   completely ignore the TSS.  We are no exception.

   Unfortunately, there is one thing that can only be done using
   a TSS: stack switching for interrupts that occur in user mode.
   When an interrupt occurs in user mode (ring 3), the processor
   consults the ss0 and esp0 members of the current TSS to
   determine the stack to use for handling the interrupt.  Thus,
   we must create a TSS and initialize at least these fields, and
   this is precisely what this file does.

   When an interrupt is handled by an interrupt or trap gate
   (which applies to all interrupts we handle), an x86 processor
   works like this:

     - If the code interrupted by the interrupt is in the same
       ring as the interrupt handler, then no stack switch takes
       place.  This is the case for interrupts that happen when
       we're running in the kernel.  The contents of the TSS are
       irrelevant for this case.

     - If the interrupted code is in a different ring from the
       handler, then the processor switches to the stack
       specified in the TSS for the new ring.  This is the case
       for interrupts that happen when we're in user space.  It's
       important that we switch to a stack that's not already in
       use, to avoid corruption.  Because we're running in user
       space, we know that the current process's kernel stack is
       not in use, so we can always use that.  Thus, when the
       scheduler switches threads, it also changes the TSS's
       stack pointer to point to the new thread's kernel stack.
       (The call is in thread_schedule_tail() in thread.c.)

   See [IA32-v3a] 6.2.1 "Task-State Segment (TSS)" for a
   description of the TSS.  See [IA32-v3a] 5.12.1 "Exception- or
   Interrupt-Handler Procedures" for a description of when and
   how stack switching occurs during an interrupt. */
struct tss
  {
    uint16_t back_link, :16;
    void *esp0;                         /* Ring 0 stack virtual address. */
    uint16_t ss0, :16;                  /* Ring 0 stack segment selector. */
    void *esp1;
    uint16_t ss1, :16;
    void *esp2;
    uint16_t ss2, :16;
    uint32_t cr3;
    void (*eip) (void);
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint16_t es, :16;
    uint16_t cs, :16;
    uint16_t ss, :16;
    uint16_t ds, :16;
    uint16_t fs, :16;
    uint16_t gs, :16;
    uint16_t ldt, :16;
    uint16_t trace, bitmap;
  };

/* Kernel TSS. */
static struct tss *tss;

/* Initializes the kernel TSS. */
void
tss_init (void) 
{
  /* Our TSS is never used in a call gate or task gate, so only a
     few fields of it are ever referenced, and those are the only
     ones we initialize. */
  tss = palloc_get_page (PAL_ASSERT | PAL_ZERO);
  tss->ss0 = SEL_KDSEG;
  tss->bitmap = 0xdfff;
  tss_update ();
}

/* Returns the kernel TSS. */
struct tss *
tss_get (void) 
{
  ASSERT (tss != NULL);
  return tss;
}

/* Sets the ring 0 stack pointer in the TSS to point to the end
   of the thread stack. */
void
tss_update (void) 
{
  ASSERT (tss != NULL);
  tss->esp0 = (uint8_t *) thread_current () + PGSIZE;
}

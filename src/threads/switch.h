#ifndef THREADS_SWITCH_H
#define THREADS_SWITCH_H

#ifndef __ASSEMBLER__
/* switch_thread()'s stack frame. */
struct switch_threads_frame 
  {
    uint32_t edi;               /*  0: Saved %edi. */
    uint32_t esi;               /*  4: Saved %esi. */
    uint32_t ebp;               /*  8: Saved %ebp. */
    uint32_t ebx;               /* 12: Saved %ebx. */
    void (*eip) (void);         /* 16: Return address. */
    struct thread *cur;         /* 20: switch_threads()'s CUR argument. */
    struct thread *next;        /* 24: switch_threads()'s NEXT argument. */
  };

/* Switches from CUR, which must be the running thread, to NEXT,
   which must also be running switch_threads(), returning CUR in
   NEXT's context. */
struct thread *switch_threads (struct thread *cur, struct thread *next);

/* Stack frame for switch_entry(). */
struct switch_entry_frame
  {
    void (*eip) (void);
  };

void switch_entry (void);

/* Pops the CUR and NEXT arguments off the stack, for use in
   initializing threads. */
void switch_thunk (void);
#endif

/* Offsets used by switch.S. */
#define SWITCH_CUR      20
#define SWITCH_NEXT     24

#endif /* threads/switch.h */

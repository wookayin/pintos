#include <hash.h>
#include "lib/kernel/hash.h"

#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"


/* A global lock, to ensure critical sections on frame operations. */
static struct lock frame_lock;

/* A mapping from physical address to frame table entry. */
static struct hash frame_map;

static unsigned frame_hash_func(const struct hash_elem *elem, void *aux);
static bool     frame_less_func(const struct hash_elem *, const struct hash_elem *, void *aux);

/**
 * Frame Table Entry
 */
struct frame_table_entry
  {
    void *physical_addr;       /* Physical Address (= Kernel Address, in PintOS) */

    struct hash_elem elem;     /* see ::frame_map */

    void *upage;               /* User (Virtual Memory) Address, pointer to page */
    struct thread *t;          /* The associated thread. */
  };


void
vm_frame_init ()
{
  lock_init (&frame_lock);
  hash_init (&frame_map, frame_hash_func, frame_less_func, NULL);
}

/**
 * Allocate a new frame, and return the address of the associated page
 * on user's virtual memory.
 */
void*
vm_frame_allocate (enum palloc_flags flags)
{
  void *vpage = palloc_get_page (PAL_USER | flags);
  if (vpage == NULL) {
    // page allocation failed. need swappping out after
    return NULL;
  }

  struct frame_table_entry *frame = malloc(sizeof(struct frame_table_entry));
  if(frame == NULL) {
    // frame allocation failed. a critical state or panic?
    return NULL;
  }

  frame->t = thread_current ();
  frame->upage = vpage;                       // the virtual address
  frame->physical_addr = (void*) vtop(vpage); // the associated physical address

  // insert into hash table
  lock_acquire (&frame_lock);
  hash_insert (&frame_map, &frame->elem);
  lock_release (&frame_lock);

  return vpage;
}

/**
 * Deallocate a frame or page.
 */
void
vm_frame_free (void *vpage)
{
  // check page-aligned
  if ( (PGMASK & (unsigned)vpage) == 0 ) {
    PANIC ("vm_frame_free is not aligned - aborting");
  }

  // hash lookup : a temporary entry
  struct frame_table_entry *f = (struct frame_table_entry*) malloc(sizeof(struct frame_table_entry));
  f->physical_addr = (void*) vtop (vpage);

  struct hash_elem *h = hash_find (&frame_map, &f->elem);
  free(f);
  if (h == NULL) {
    PANIC ("The page to be freed is not stored in the table");
  }

  f = hash_entry(h, struct frame_table_entry, elem);

  lock_acquire (&frame_lock);
  hash_delete (&frame_map, &f->elem);
  lock_release (&frame_lock);

  // Free resources
  palloc_free_page (f->upage);
  free(f);
}


/* Helpers */

// Hash Functions required for [frame_map]. Uses 'kaddr' as key.
static unsigned frame_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct frame_table_entry *entry = hash_entry(elem, struct frame_table_entry, elem);
  return hash_bytes( &entry->physical_addr, sizeof entry->physical_addr );
}
static bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct frame_table_entry *a_entry = hash_entry(a, struct frame_table_entry, elem);
  struct frame_table_entry *b_entry = hash_entry(b, struct frame_table_entry, elem);
  return a_entry->physical_addr < b_entry->physical_addr;
}

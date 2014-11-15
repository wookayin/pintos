#include <hash.h>
#include <stdio.h>
#include "lib/kernel/hash.h"

#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
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
    void *kpage;               /* Kernel page, mapped to physical address */

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
 * Allocate a new frame,
 * and return the address of the associated page.
 */
void*
vm_frame_allocate (enum palloc_flags flags, void *upage)
{
  void *frame_page = palloc_get_page (PAL_USER | flags);
  if (frame_page == NULL) {
    // page allocation failed. need swappping out after
    return NULL;
  }

  struct frame_table_entry *frame = malloc(sizeof(struct frame_table_entry));
  if(frame == NULL) {
    // frame allocation failed. a critical state or panic?
    return NULL;
  }

  frame->t = thread_current ();
  frame->upage = upage;
  frame->kpage = frame_page;

  // insert into hash table
  lock_acquire (&frame_lock);
  hash_insert (&frame_map, &frame->elem);
  lock_release (&frame_lock);

  return frame_page;
}

/**
 * Deallocate a frame or page.
 */
void
vm_frame_free (void *kpage)
{
  ASSERT (is_kernel_vaddr(kpage));
  ASSERT (pg_ofs (kpage) == 0); // should be aligned

  // hash lookup : a temporary entry
  struct frame_table_entry f_tmp;
  f_tmp.kpage = kpage;

  struct hash_elem *h = hash_find (&frame_map, &(f_tmp.elem));
  if (h == NULL) {
    PANIC ("The page to be freed is not stored in the table");
  }

  struct frame_table_entry *f;
  f = hash_entry(h, struct frame_table_entry, elem);

  lock_acquire (&frame_lock);
  hash_delete (&frame_map, &f->elem);
  lock_release (&frame_lock);

  // Free resources
  palloc_free_page (kpage);
}


/* Helpers */

// Hash Functions required for [frame_map]. Uses 'kpage' as key.
static unsigned frame_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct frame_table_entry *entry = hash_entry(elem, struct frame_table_entry, elem);
  return hash_bytes( &entry->kpage, sizeof entry->kpage );
}
static bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct frame_table_entry *a_entry = hash_entry(a, struct frame_table_entry, elem);
  struct frame_table_entry *b_entry = hash_entry(b, struct frame_table_entry, elem);
  return a_entry->kpage < b_entry->kpage;
}

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

    bool pinned;               /* Used to prevent a frame from being evicted, while it is acquiring some resources.
                                  If it is true, it is never evicted. */
  };


struct frame_table_entry* pick_frame_to_evict(void);

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
    // page allocation failed.

    /* first, swap out the page */
    struct frame_table_entry *f_evicted = pick_frame_to_evict();
    ASSERT (f_evicted->t != NULL);

    // clear the page mapping, and replace it with swap
    pagedir_clear_page(f_evicted->t->pagedir, f_evicted->upage);

    swap_index_t swap_idx = vm_swap_out( f_evicted->kpage );
    vm_supt_set_swap(f_evicted->t->supt, f_evicted->upage, swap_idx);

    /* update the page table, and free the frame table */
    vm_frame_free(f_evicted->kpage);

    frame_page = palloc_get_page (PAL_USER | flags);
    ASSERT (frame_page != NULL); // should success in this chance
  }

  struct frame_table_entry *frame = malloc(sizeof(struct frame_table_entry));
  if(frame == NULL) {
    // frame allocation failed. a critical state or panic?
    return NULL;
  }

  frame->t = thread_current ();
  frame->upage = upage;
  frame->kpage = frame_page;
  frame->pinned = true;         // can't be evicted yet

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
  palloc_free_page(kpage);
  free(f);
}

/** Frame Eviction Strategy */
struct frame_table_entry* pick_frame_to_evict(void)
{
  // TODO : implement clock-algorithm

  // as of now, use the simplest one -- random!
  size_t n = hash_size(&frame_map);
  static unsigned prng = 1;

  while(true) {
    prng = prng * 1664525u + 1013904223u;
    size_t pointer = prng % n;

    struct hash_iterator it; hash_first(&it, &frame_map);
    size_t i; for(i=0; i<=pointer; ++i) hash_next(&it);

    struct frame_table_entry *e = hash_entry(hash_cur(&it), struct frame_table_entry, elem);
    if(e->pinned) { // it is pinned. try again
      printf("pinned, continue\n");
      continue;
    }
    else return e;  // unpinned. evict it!
  }
}


void
vm_frame_unpin (void* kpage)
{
  lock_acquire (&frame_lock);

  // hash lookup : a temporary entry
  struct frame_table_entry f_tmp;
  f_tmp.kpage = kpage;
  struct hash_elem *h = hash_find (&frame_map, &(f_tmp.elem));
  if (h == NULL) {
    PANIC ("The frame to be unpinned does not exist");
  }

  struct frame_table_entry *f;
  f = hash_entry(h, struct frame_table_entry, elem);
  f->pinned = false; // unpin.

  lock_release (&frame_lock);
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

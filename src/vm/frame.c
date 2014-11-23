#include <hash.h>
#include <list.h>
#include <stdio.h>
#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"

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

/* A (circular) list of frames for the clock eviction algorithm. */
static struct list frame_list;      /* the list */
static struct list_elem *clock_ptr; /* the pointer in clock algorithm */

static unsigned frame_hash_func(const struct hash_elem *elem, void *aux);
static bool     frame_less_func(const struct hash_elem *, const struct hash_elem *, void *aux);

/**
 * Frame Table Entry
 */
struct frame_table_entry
  {
    void *kpage;               /* Kernel page, mapped to physical address */

    struct hash_elem helem;    /* see ::frame_map */
    struct list_elem lelem;    /* see ::frame_list */

    void *upage;               /* User (Virtual Memory) Address, pointer to page */
    struct thread *t;          /* The associated thread. */

    bool pinned;               /* Used to prevent a frame from being evicted, while it is acquiring some resources.
                                  If it is true, it is never evicted. */
  };


static struct frame_table_entry* pick_frame_to_evict(uint32_t* pagedir);
static void vm_frame_do_free (void *kpage, bool free_page);


void
vm_frame_init ()
{
  lock_init (&frame_lock);
  hash_init (&frame_map, frame_hash_func, frame_less_func, NULL);
  list_init (&frame_list);
  clock_ptr = NULL;
}

/**
 * Allocate a new frame,
 * and return the address of the associated page.
 */
void*
vm_frame_allocate (enum palloc_flags flags, void *upage)
{
  lock_acquire (&frame_lock);

  void *frame_page = palloc_get_page (PAL_USER | flags);
  if (frame_page == NULL) {
    // page allocation failed.

    /* first, swap out the page */
    struct frame_table_entry *f_evicted = pick_frame_to_evict( thread_current()->pagedir );

#if DEBUG
    printf("f_evicted: %x th=%x, pagedir = %x, up = %x, kp = %x, hash_size=%d\n", f_evicted, f_evicted->t,
        f_evicted->t->pagedir, f_evicted->upage, f_evicted->kpage, hash_size(&frame_map));
#endif
    ASSERT (f_evicted != NULL && f_evicted->t != NULL);

    // clear the page mapping, and replace it with swap
    ASSERT (f_evicted->t->pagedir != (void*)0xcccccccc);
    pagedir_clear_page(f_evicted->t->pagedir, f_evicted->upage);

    bool is_dirty = false;
    is_dirty = is_dirty || pagedir_is_dirty(f_evicted->t->pagedir, f_evicted->upage);
    is_dirty = is_dirty || pagedir_is_dirty(f_evicted->t->pagedir, f_evicted->kpage);

    swap_index_t swap_idx = vm_swap_out( f_evicted->kpage );
    vm_supt_set_swap(f_evicted->t->supt, f_evicted->upage, swap_idx);
    vm_supt_set_dirty(f_evicted->t->supt, f_evicted->upage, is_dirty);
    vm_frame_do_free(f_evicted->kpage, true); // f_evicted is also invalidated

    frame_page = palloc_get_page (PAL_USER | flags);
    ASSERT (frame_page != NULL); // should success in this chance
  }

  struct frame_table_entry *frame = malloc(sizeof(struct frame_table_entry));
  if(frame == NULL) {
    // frame allocation failed. a critical state or panic?
    lock_release (&frame_lock);
    return NULL;
  }

  frame->t = thread_current ();
  frame->upage = upage;
  frame->kpage = frame_page;
  frame->pinned = true;         // can't be evicted yet

  // insert into hash table
  hash_insert (&frame_map, &frame->helem);
  list_push_back (&frame_list, &frame->lelem);

  lock_release (&frame_lock);
  return frame_page;
}

/**
 * Deallocate a frame or page.
 */
void
vm_frame_free (void *kpage)
{
  lock_acquire (&frame_lock);
  vm_frame_do_free (kpage, true);
  lock_release (&frame_lock);
}

/**
 * Just removes then entry from table, do not palloc free.
 */
void
vm_frame_remove_entry (void *kpage)
{
  lock_acquire (&frame_lock);
  vm_frame_do_free (kpage, false);
  lock_release (&frame_lock);
}

/**
 * An (internal, private) method --
 * Deallocates a frame or page (internal procedure)
 * MUST BE CALLED with 'frame_lock' held.
 */
void
vm_frame_do_free (void *kpage, bool free_page)
{
  ASSERT (lock_held_by_current_thread(&frame_lock) == true);
  ASSERT (is_kernel_vaddr(kpage));
  ASSERT (pg_ofs (kpage) == 0); // should be aligned

  // hash lookup : a temporary entry
  struct frame_table_entry f_tmp;
  f_tmp.kpage = kpage;

  struct hash_elem *h = hash_find (&frame_map, &(f_tmp.helem));
  if (h == NULL) {
    PANIC ("The page to be freed is not stored in the table");
  }

  struct frame_table_entry *f;
  f = hash_entry(h, struct frame_table_entry, helem);

  hash_delete (&frame_map, &f->helem);
  list_remove (&f->lelem);

  // Free resources
  if(free_page) palloc_free_page(kpage);
  free(f);
}

/** Frame Eviction Strategy : The Clock Algorithm */
struct frame_table_entry* clock_frame_next(void);
struct frame_table_entry* pick_frame_to_evict( uint32_t *pagedir )
{
  size_t n = hash_size(&frame_map);
  if(n == 0) PANIC("Frame table is empty, can't happen - there is a leak somewhere");

  size_t it;
  for(it = 0; it <= n + n; ++ it) // prevent infinite loop. 2n iterations is enough
  {
    struct frame_table_entry *e = clock_frame_next();
    // if pinned, continue
    if(e->pinned) continue;
    // if referenced, give a second chance.
    else if( pagedir_is_accessed(pagedir, e->upage)) {
      pagedir_set_accessed(pagedir, e->upage, false);
      continue;
    }

    // OK, here is the victim : unreferenced since its last chance
    return e;
  }

  PANIC ("Can't evict any frame -- Not enough memory!\n");
}
struct frame_table_entry* clock_frame_next(void)
{
  if (list_empty(&frame_list))
    PANIC("Frame table is empty, can't happen - there is a leak somewhere");

  if (clock_ptr == NULL || clock_ptr == list_end(&frame_list))
    clock_ptr = list_begin (&frame_list);
  else
    clock_ptr = list_next (clock_ptr);

  struct frame_table_entry *e = list_entry(clock_ptr, struct frame_table_entry, lelem);
  return e;
}


static void
vm_frame_set_pinned (void *kpage, bool new_value)
{
  lock_acquire (&frame_lock);

  // hash lookup : a temporary entry
  struct frame_table_entry f_tmp;
  f_tmp.kpage = kpage;
  struct hash_elem *h = hash_find (&frame_map, &(f_tmp.helem));
  if (h == NULL) {
    PANIC ("The frame to be pinned/unpinned does not exist");
  }

  struct frame_table_entry *f;
  f = hash_entry(h, struct frame_table_entry, helem);
  f->pinned = new_value;

  lock_release (&frame_lock);
}

void
vm_frame_unpin (void* kpage) {
  vm_frame_set_pinned (kpage, false);
}

void
vm_frame_pin (void* kpage) {
  vm_frame_set_pinned (kpage, true);
}


/* Helpers */

// Hash Functions required for [frame_map]. Uses 'kpage' as key.
static unsigned frame_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct frame_table_entry *entry = hash_entry(elem, struct frame_table_entry, helem);
  return hash_bytes( &entry->kpage, sizeof entry->kpage );
}
static bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct frame_table_entry *a_entry = hash_entry(a, struct frame_table_entry, helem);
  struct frame_table_entry *b_entry = hash_entry(b, struct frame_table_entry, helem);
  return a_entry->kpage < b_entry->kpage;
}

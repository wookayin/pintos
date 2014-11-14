#include <hash.h>
#include "lib/kernel/hash.h"

#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "vm/page.h"

static unsigned spte_hash_func(const struct hash_elem *elem, void *aux);
static bool     spte_less_func(const struct hash_elem *, const struct hash_elem *, void *aux);
static void     spte_destroy_func(struct hash_elem *elem, void *aux);


struct supplemental_page_table*
vm_supt_create (void)
{
  struct supplemental_page_table *supt =
    (struct supplemental_page_table*) malloc(sizeof(struct supplemental_page_table));

  hash_init (&supt->page_map, spte_hash_func, spte_less_func, NULL);
  return supt;
}

void
vm_supt_destroy (struct supplemental_page_table *supt)
{
  ASSERT (supt != NULL);

  hash_destroy (&supt->page_map, spte_destroy_func);
  free (supt);
}


/**
 * Install a page (specified by the starting address `upage`) which
 * is currently on the frame, in the supplemental page table.
 *
 * Returns true if successful, false otherwise.
 * (In case of failure, a proper handling is required later -- process.c)
 */
bool
vm_supt_set_page (struct supplemental_page_table *supt, void *upage)
{
  struct supplemental_page_table_entry *spte;
  spte = (struct supplemental_page_table_entry *) malloc(sizeof(struct supplemental_page_table_entry));

  spte->upage = upage;
  spte->status = ON_FRAME;

  struct hash_elem *prev_elem;
  prev_elem = hash_insert (&supt->page_map, &spte->elem);
  if (prev_elem == NULL) {
    // successfully inserted into the supplemental page table.
    return true;
  }
  else {
    // failed. there is already an entry.
    free (spte);
    return false;
  }
}



/* Helpers */

// Hash Functions required for [frame_map]. Uses 'kaddr' as key.
static unsigned
spte_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct supplemental_page_table_entry *entry = hash_entry(elem, struct supplemental_page_table_entry, elem);
  return hash_int( (int)entry->upage );
}
static bool
spte_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct supplemental_page_table_entry *a_entry = hash_entry(a, struct supplemental_page_table_entry, elem);
  struct supplemental_page_table_entry *b_entry = hash_entry(b, struct supplemental_page_table_entry, elem);
  return a_entry->upage < b_entry->upage;
}
static void
spte_destroy_func(struct hash_elem *elem, void *aux UNUSED)
{
  struct supplemental_page_table_entry *entry = hash_entry(elem, struct supplemental_page_table_entry, elem);
  free (entry);
}

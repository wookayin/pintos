#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

/**
 * Indicates a state of page.
 */
enum page_status {
  ALL_ZERO,         // All zeros
  ON_FRAME,         // Actively in memory
  ON_SWAP           // Swapped (on swap slot)
};

/**
 * Supplemental page table. The scope is per-process.
 */
struct supplemental_page_table
  {
    /* The hash table, page -> spte */
    struct hash page_map;
  };

struct supplemental_page_table_entry
  {
    void *upage;              /* Virtual address of the page (the key) */
    struct hash_elem elem;

    enum page_status status;
  };


/*
 * Methods for manipulating supplemental page tables.
 */

struct supplemental_page_table* vm_supt_create (void);
void vm_supt_destroy (struct supplemental_page_table *);

bool vm_supt_set_page (struct supplemental_page_table *supt, void *);

struct supplemental_page_table_entry* vm_supt_lookup (struct supplemental_page_table *supt, void *);
bool vm_supt_is_valid (struct supplemental_page_table *, void *page);

bool vm_load_page(struct supplemental_page_table *supt, uint32_t *pagedir, void *upage);

#endif

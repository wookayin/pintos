#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

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
  };


/*
 * Methods for manipulating supplemental page tables.
 */

struct supplemental_page_table* vm_supt_create (void);
void vm_supt_destroy (struct supplemental_page_table *);

#endif

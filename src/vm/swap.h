#ifndef VM_SWAP_H
#define VM_SWAP_H

typedef uint32_t swap_index_t;


/* Functions for Swap Table manipulation. */

/**
 * Initialize the swap. Must be called ONLY ONCE at the initializtion phase.
 */
void vm_swap_init (void);

/**
 * Swap Out: write the content of `page` into the swap disk,
 * and return the index of swap region in which it is placed.
 */
swap_index_t vm_swap_out (void *page);

/**
 * Swap In: read the content of from the specified swap index,
 * from the mapped swap block, and store PGSIZE bytes into `page`.
 */
void vm_swap_in (swap_index_t swap_index, void *page);

/**
 * Free Swap: drop the swap region.
 */
void vm_swap_free (swap_index_t swap_index);


#endif /* vm/swap.h */

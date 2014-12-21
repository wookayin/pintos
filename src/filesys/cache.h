#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"

/* Buffer Caches. */

void buffer_cache_init (void);
void buffer_cache_close (void);

/**
 * Read SECTOR_SIZE bytes of data starting from the disk sector
 * specified by 'sector', into `target` (user memory address).
 */
void buffer_cache_read (block_sector_t sector, void *target);

/**
 * Writes SECTOR_SIZE bytes of data into the disk sector
 * specified by 'sector', from `source` (user memory address).
 */
void buffer_cache_write (block_sector_t sector, const void *source);

#endif

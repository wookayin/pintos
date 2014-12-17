#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/synch.h"

#define BUFFER_CACHE_SIZE 64

struct buffer_cache_entry_t {
  bool occupied;  // true only if this entry is valid cache entry

  block_sector_t disk_sector;
  uint8_t buffer[BLOCK_SECTOR_SIZE];

  bool dirty;     // dirty bit
  bool access;    // reference bit, for clock algorithm
};

/* Buffer cache entries. */
static struct buffer_cache_entry_t cache[BUFFER_CACHE_SIZE];

/* A global lock for synchronizing buffer cache operations. */
static struct lock buffer_cache_lock;

void
buffer_cache_init (void)
{
  lock_init (&buffer_cache_lock);

  // initialize entries
  size_t i;
  for (i = 0; i < BUFFER_CACHE_SIZE; ++ i)
  {
    cache[i].occupied = false;
  }
}

void
buffer_cache_close (void)
{
  // TODO flush buffer cache entries
}

void
buffer_cache_read (block_sector_t sector, void *target)
{
  // TODO need implement
  block_read (fs_device, sector, target);
}

void
buffer_cache_write (block_sector_t sector, void *source)
{
  // TODO need implement
  block_write (fs_device, sector, source);
}

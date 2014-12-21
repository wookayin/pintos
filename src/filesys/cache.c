#include <debug.h>
#include <string.h>
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

/**
 * An internal method for flushing back the cache entry into disk.
 * Must be called with the lock held.
 */
static void
buffer_cache_flush (struct buffer_cache_entry_t *entry)
{
  ASSERT (lock_held_by_current_thread(&buffer_cache_lock));
  ASSERT (entry != NULL && entry->occupied == true);

  if (entry->dirty) {
    block_write (fs_device, entry->disk_sector, entry->buffer);
    entry->dirty = false;
  }
}

void
buffer_cache_close (void)
{
  // flush buffer cache entries
  lock_acquire (&buffer_cache_lock);

  size_t i;
  for (i = 0; i < BUFFER_CACHE_SIZE; ++ i)
  {
    if (cache[i].occupied == false) continue;
    buffer_cache_flush( &(cache[i]) );
  }

  lock_release (&buffer_cache_lock);
}


/**
 * Lookup the cache entry, and returns the pointer of buffer_cache_entry_t,
 * or NULL in case of cache miss. (simply traverse the cache entries)
 */
static struct buffer_cache_entry_t*
buffer_cache_lookup (block_sector_t sector)
{
  size_t i;
  for (i = 0; i < BUFFER_CACHE_SIZE; ++ i)
  {
    if (cache[i].occupied == false) continue;
    if (cache[i].disk_sector == sector) {
      // cache hit.
      return &(cache[i]);
    }
  }
  return NULL; // cache miss
}

/**
 * Obtain a free cache entry slot.
 * If there is an unoccupied slot already, return it.
 * Otherwise, some entry should be evicted by the clock algorithm.
 */
static struct buffer_cache_entry_t*
buffer_cache_evict (void)
{
  ASSERT (lock_held_by_current_thread(&buffer_cache_lock));

  // clock algorithm
  static size_t clock = 0;
  while (true) {
    if (cache[clock].occupied == false) {
      // found an empty slot -- use it
      return &(cache[clock]);
    }

    if (cache[clock].access) {
      // give a second chance
      cache[clock].access = false;
    }
    else break;

    clock ++;
    clock %= BUFFER_CACHE_SIZE;
  }

  // evict cache[clock]
  struct buffer_cache_entry_t *slot = &cache[clock];
  if (slot->dirty) {
    // write back into disk
    buffer_cache_flush (slot);
  }

  slot->occupied = false;
  return slot;
}


void
buffer_cache_read (block_sector_t sector, void *target)
{
  lock_acquire (&buffer_cache_lock);

  struct buffer_cache_entry_t *slot = buffer_cache_lookup (sector);
  if (slot == NULL) {
    // cache miss: need eviction.
    slot = buffer_cache_evict ();
    ASSERT (slot != NULL && slot->occupied == false);

    // fill in the cache entry.
    slot->occupied = true;
    slot->disk_sector = sector;
    slot->dirty = false;
    block_read (fs_device, sector, slot->buffer);
  }

  // copy the buffer data into memory.
  slot->access = true;
  memcpy (target, slot->buffer, BLOCK_SECTOR_SIZE);

  lock_release (&buffer_cache_lock);
}

void
buffer_cache_write (block_sector_t sector, const void *source)
{
  lock_acquire (&buffer_cache_lock);

  struct buffer_cache_entry_t *slot = buffer_cache_lookup (sector);
  if (slot == NULL) {
    // cache miss: need eviction.
    slot = buffer_cache_evict ();
    ASSERT (slot != NULL && slot->occupied == false);

    // fill in the cache entry.
    slot->occupied = true;
    slot->disk_sector = sector;
    slot->dirty = false;
    block_read (fs_device, sector, slot->buffer);
  }

  // copy the data form memory into the buffer cache.
  slot->access = true;
  slot->dirty = true;
  memcpy (slot->buffer, source, BLOCK_SECTOR_SIZE);

  lock_release (&buffer_cache_lock);
}

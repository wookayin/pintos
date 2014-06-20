#include "devices/block.h"
#include <list.h>
#include <string.h>
#include <stdio.h>
#include "devices/ide.h"
#include "threads/malloc.h"

/* A block device. */
struct block
  {
    struct list_elem list_elem;         /* Element in all_blocks. */

    char name[16];                      /* Block device name. */
    enum block_type type;                /* Type of block device. */
    block_sector_t size;                 /* Size in sectors. */

    const struct block_operations *ops;  /* Driver operations. */
    void *aux;                          /* Extra data owned by driver. */

    unsigned long long read_cnt;        /* Number of sectors read. */
    unsigned long long write_cnt;       /* Number of sectors written. */
  };

/* List of all block devices. */
static struct list all_blocks = LIST_INITIALIZER (all_blocks);

/* The block block assigned to each Pintos role. */
static struct block *block_by_role[BLOCK_ROLE_CNT];

static struct block *list_elem_to_block (struct list_elem *);

/* Returns a human-readable name for the given block device
   TYPE. */
const char *
block_type_name (enum block_type type)
{
  static const char *block_type_names[BLOCK_CNT] =
    {
      "kernel",
      "filesys",
      "scratch",
      "swap",
      "raw",
      "foreign",
    };

  ASSERT (type < BLOCK_CNT);
  return block_type_names[type];
}

/* Returns the block device fulfilling the given ROLE, or a null
   pointer if no block device has been assigned that role. */
struct block *
block_get_role (enum block_type role)
{
  ASSERT (role < BLOCK_ROLE_CNT);
  return block_by_role[role];
}

/* Assigns BLOCK the given ROLE. */
void
block_set_role (enum block_type role, struct block *block)
{
  ASSERT (role < BLOCK_ROLE_CNT);
  block_by_role[role] = block;
}

/* Returns the first block device in kernel probe order, or a
   null pointer if no block devices are registered. */
struct block *
block_first (void)
{
  return list_elem_to_block (list_begin (&all_blocks));
}

/* Returns the block device following BLOCK in kernel probe
   order, or a null pointer if BLOCK is the last block device. */
struct block *
block_next (struct block *block)
{
  return list_elem_to_block (list_next (&block->list_elem));
}

/* Returns the block device with the given NAME, or a null
   pointer if no block device has that name. */
struct block *
block_get_by_name (const char *name)
{
  struct list_elem *e;

  for (e = list_begin (&all_blocks); e != list_end (&all_blocks);
       e = list_next (e))
    {
      struct block *block = list_entry (e, struct block, list_elem);
      if (!strcmp (name, block->name))
        return block;
    }

  return NULL;
}

/* Verifies that SECTOR is a valid offset within BLOCK.
   Panics if not. */
static void
check_sector (struct block *block, block_sector_t sector)
{
  if (sector >= block->size)
    {
      /* We do not use ASSERT because we want to panic here
         regardless of whether NDEBUG is defined. */
      PANIC ("Access past end of device %s (sector=%"PRDSNu", "
             "size=%"PRDSNu")\n", block_name (block), sector, block->size);
    }
}

/* Reads sector SECTOR from BLOCK into BUFFER, which must
   have room for BLOCK_SECTOR_SIZE bytes.
   Internally synchronizes accesses to block devices, so external
   per-block device locking is unneeded. */
void
block_read (struct block *block, block_sector_t sector, void *buffer)
{
  check_sector (block, sector);
  block->ops->read (block->aux, sector, buffer);
  block->read_cnt++;
}

/* Write sector SECTOR to BLOCK from BUFFER, which must contain
   BLOCK_SECTOR_SIZE bytes.  Returns after the block device has
   acknowledged receiving the data.
   Internally synchronizes accesses to block devices, so external
   per-block device locking is unneeded. */
void
block_write (struct block *block, block_sector_t sector, const void *buffer)
{
  check_sector (block, sector);
  ASSERT (block->type != BLOCK_FOREIGN);
  block->ops->write (block->aux, sector, buffer);
  block->write_cnt++;
}

/* Returns the number of sectors in BLOCK. */
block_sector_t
block_size (struct block *block)
{
  return block->size;
}

/* Returns BLOCK's name (e.g. "hda"). */
const char *
block_name (struct block *block)
{
  return block->name;
}

/* Returns BLOCK's type. */
enum block_type
block_type (struct block *block)
{
  return block->type;
}

/* Prints statistics for each block device used for a Pintos role. */
void
block_print_stats (void)
{
  int i;

  for (i = 0; i < BLOCK_ROLE_CNT; i++)
    {
      struct block *block = block_by_role[i];
      if (block != NULL)
        {
          printf ("%s (%s): %llu reads, %llu writes\n",
                  block->name, block_type_name (block->type),
                  block->read_cnt, block->write_cnt);
        }
    }
}

/* Registers a new block device with the given NAME.  If
   EXTRA_INFO is non-null, it is printed as part of a user
   message.  The block device's SIZE in sectors and its TYPE must
   be provided, as well as the it operation functions OPS, which
   will be passed AUX in each function call. */
struct block *
block_register (const char *name, enum block_type type,
                const char *extra_info, block_sector_t size,
                const struct block_operations *ops, void *aux)
{
  struct block *block = malloc (sizeof *block);
  if (block == NULL)
    PANIC ("Failed to allocate memory for block device descriptor");

  list_push_back (&all_blocks, &block->list_elem);
  strlcpy (block->name, name, sizeof block->name);
  block->type = type;
  block->size = size;
  block->ops = ops;
  block->aux = aux;
  block->read_cnt = 0;
  block->write_cnt = 0;

  printf ("%s: %'"PRDSNu" sectors (", block->name, block->size);
  print_human_readable_size ((uint64_t) block->size * BLOCK_SECTOR_SIZE);
  printf (")");
  if (extra_info != NULL)
    printf (", %s", extra_info);
  printf ("\n");

  return block;
}

/* Returns the block device corresponding to LIST_ELEM, or a null
   pointer if LIST_ELEM is the list end of all_blocks. */
static struct block *
list_elem_to_block (struct list_elem *list_elem)
{
  return (list_elem != list_end (&all_blocks)
          ? list_entry (list_elem, struct block, list_elem)
          : NULL);
}


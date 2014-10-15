#ifndef DEVICES_BLOCK_H
#define DEVICES_BLOCK_H

#include <stddef.h>
#include <inttypes.h>

/* Size of a block device sector in bytes.
   All IDE disks use this sector size, as do most USB and SCSI
   disks.  It's not worth it to try to cater to other sector
   sizes in Pintos (yet). */
#define BLOCK_SECTOR_SIZE 512

/* Index of a block device sector.
   Good enough for devices up to 2 TB. */
typedef uint32_t block_sector_t;

/* Format specifier for printf(), e.g.:
   printf ("sector=%"PRDSNu"\n", sector); */
#define PRDSNu PRIu32

/* Higher-level interface for file systems, etc. */

struct block;

/* Type of a block device. */
enum block_type
  {
    /* Block device types that play a role in Pintos. */
    BLOCK_KERNEL,                /* Pintos OS kernel. */
    BLOCK_FILESYS,               /* File system. */
    BLOCK_SCRATCH,               /* Scratch. */
    BLOCK_SWAP,                  /* Swap. */
    BLOCK_ROLE_CNT,

    /* Other kinds of block devices that Pintos may see but does
       not interact with. */
    BLOCK_RAW = BLOCK_ROLE_CNT,  /* "Raw" device with unidentified contents. */
    BLOCK_FOREIGN,               /* Owned by non-Pintos operating system. */
    BLOCK_CNT                    /* Number of Pintos block types. */
  };

const char *block_type_name (enum block_type);

/* Finding block devices. */
struct block *block_get_role (enum block_type);
void block_set_role (enum block_type, struct block *);
struct block *block_get_by_name (const char *name);

struct block *block_first (void);
struct block *block_next (struct block *);

/* Block device operations. */
block_sector_t block_size (struct block *);
void block_read (struct block *, block_sector_t, void *);
void block_write (struct block *, block_sector_t, const void *);
const char *block_name (struct block *);
enum block_type block_type (struct block *);

/* Statistics. */
void block_print_stats (void);

/* Lower-level interface to block device drivers. */

struct block_operations
  {
    void (*read) (void *aux, block_sector_t, void *buffer);
    void (*write) (void *aux, block_sector_t, const void *buffer);
  };

struct block *block_register (const char *name, enum block_type,
                              const char *extra_info, block_sector_t size,
                              const struct block_operations *, void *aux);

#endif /* devices/block.h */

#include "devices/partition.h"
#include <packed.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "devices/block.h"
#include "threads/malloc.h"

/* A partition of a block device. */
struct partition
  {
    struct block *block;                /* Underlying block device. */
    block_sector_t start;               /* First sector within device. */
  };

static struct block_operations partition_operations;

static void read_partition_table (struct block *, block_sector_t sector,
                                  block_sector_t primary_extended_sector,
                                  int *part_nr);
static void found_partition (struct block *, uint8_t type,
                             block_sector_t start, block_sector_t size,
                             int part_nr);
static const char *partition_type_name (uint8_t);

/* Scans BLOCK for partitions of interest to Pintos. */
void
partition_scan (struct block *block)
{
  int part_nr = 0;
  read_partition_table (block, 0, 0, &part_nr);
  if (part_nr == 0)
    printf ("%s: Device contains no partitions\n", block_name (block));
}

/* Reads the partition table in the given SECTOR of BLOCK and
   scans it for partitions of interest to Pintos.

   If SECTOR is 0, so that this is the top-level partition table
   on BLOCK, then PRIMARY_EXTENDED_SECTOR is not meaningful;
   otherwise, it should designate the sector of the top-level
   extended partition table that was traversed to arrive at
   SECTOR, for use in finding logical partitions (see the large
   comment below).

   PART_NR points to the number of non-empty primary or logical
   partitions already encountered on BLOCK.  It is incremented as
   partitions are found. */
static void
read_partition_table (struct block *block, block_sector_t sector,
                      block_sector_t primary_extended_sector,
                      int *part_nr)
{
  /* Format of a partition table entry.  See [Partitions]. */
  struct partition_table_entry
    {
      uint8_t bootable;         /* 0x00=not bootable, 0x80=bootable. */
      uint8_t start_chs[3];     /* Encoded starting cylinder, head, sector. */
      uint8_t type;             /* Partition type (see partition_type_name). */
      uint8_t end_chs[3];       /* Encoded ending cylinder, head, sector. */
      uint32_t offset;          /* Start sector offset from partition table. */
      uint32_t size;            /* Number of sectors. */
    }
  PACKED;

  /* Partition table sector. */
  struct partition_table
    {
      uint8_t loader[446];      /* Loader, in top-level partition table. */
      struct partition_table_entry partitions[4];       /* Table entries. */
      uint16_t signature;       /* Should be 0xaa55. */
    }
  PACKED;

  struct partition_table *pt;
  size_t i;

  /* Check SECTOR validity. */
  if (sector >= block_size (block))
    {
      printf ("%s: Partition table at sector %"PRDSNu" past end of device.\n",
              block_name (block), sector);
      return;
    }

  /* Read sector. */
  ASSERT (sizeof *pt == BLOCK_SECTOR_SIZE);
  pt = malloc (sizeof *pt);
  if (pt == NULL)
    PANIC ("Failed to allocate memory for partition table.");
  block_read (block, 0, pt);

  /* Check signature. */
  if (pt->signature != 0xaa55)
    {
      if (primary_extended_sector == 0)
        printf ("%s: Invalid partition table signature\n", block_name (block));
      else
        printf ("%s: Invalid extended partition table in sector %"PRDSNu"\n",
                block_name (block), sector);
      free (pt);
      return;
    }

  /* Parse partitions. */
  for (i = 0; i < sizeof pt->partitions / sizeof *pt->partitions; i++)
    {
      struct partition_table_entry *e = &pt->partitions[i];

      if (e->size == 0 || e->type == 0)
        {
          /* Ignore empty partition. */
        }
      else if (e->type == 0x05       /* Extended partition. */
               || e->type == 0x0f    /* Windows 98 extended partition. */
               || e->type == 0x85    /* Linux extended partition. */
               || e->type == 0xc5)   /* DR-DOS extended partition. */
        {
          printf ("%s: Extended partition in sector %"PRDSNu"\n",
                  block_name (block), sector);

          /* The interpretation of the offset field for extended
             partitions is bizarre.  When the extended partition
             table entry is in the master boot record, that is,
             the device's primary partition table in sector 0, then
             the offset is an absolute sector number.  Otherwise,
             no matter how deep the partition table we're reading
             is nested, the offset is relative to the start of
             the extended partition that the MBR points to. */
          if (sector == 0)
            read_partition_table (block, e->offset, e->offset, part_nr);
          else
            read_partition_table (block, e->offset + primary_extended_sector,
                                  primary_extended_sector, part_nr);
        }
      else
        {
          ++*part_nr;

          found_partition (block, e->type, e->offset + sector,
                           e->size, *part_nr);
        }
    }

  free (pt);
}

/* We have found a primary or logical partition of the given TYPE
   on BLOCK, starting at sector START and continuing for SIZE
   sectors, which we are giving the partition number PART_NR.
   Check whether this is a partition of interest to Pintos, and
   if so then add it to the proper element of partitions[]. */
static void
found_partition (struct block *block, uint8_t part_type,
                 block_sector_t start, block_sector_t size,
                 int part_nr)
{
  if (start >= block_size (block))
    printf ("%s%d: Partition starts past end of device (sector %"PRDSNu")\n",
            block_name (block), part_nr, start);
  else if (start + size < start || start + size > block_size (block))
    printf ("%s%d: Partition end (%"PRDSNu") past end of device (%"PRDSNu")\n",
            block_name (block), part_nr, start + size, block_size (block));
  else
    {
      enum block_type type = (part_type == 0x20 ? BLOCK_KERNEL
                              : part_type == 0x21 ? BLOCK_FILESYS
                              : part_type == 0x22 ? BLOCK_SCRATCH
                              : part_type == 0x23 ? BLOCK_SWAP
                              : BLOCK_FOREIGN);
      struct partition *p;
      char extra_info[128];
      char name[16];

      p = malloc (sizeof *p);
      if (p == NULL)
        PANIC ("Failed to allocate memory for partition descriptor");
      p->block = block;
      p->start = start;

      snprintf (name, sizeof name, "%s%d", block_name (block), part_nr);
      snprintf (extra_info, sizeof extra_info, "%s (%02x)",
                partition_type_name (part_type), part_type);
      block_register (name, type, extra_info, size, &partition_operations, p);
    }
}

/* Returns a human-readable name for the given partition TYPE. */
static const char *
partition_type_name (uint8_t type)
{
  /* Name of each known type of partition.
     From util-linux-2.12r/fdisk/i386_sys_types.c.
     This initializer makes use of a C99 feature that allows
     array elements to be initialized by index. */
  static const char *type_names[256] =
    {
      [0x00] = "Empty",
      [0x01] = "FAT12",
      [0x02] = "XENIX root",
      [0x03] = "XENIX usr",
      [0x04] = "FAT16 <32M",
      [0x05] = "Extended",
      [0x06] = "FAT16",
      [0x07] = "HPFS/NTFS",
      [0x08] = "AIX",
      [0x09] = "AIX bootable",
      [0x0a] = "OS/2 Boot Manager",
      [0x0b] = "W95 FAT32",
      [0x0c] = "W95 FAT32 (LBA)",
      [0x0e] = "W95 FAT16 (LBA)",
      [0x0f] = "W95 Ext'd (LBA)",
      [0x10] = "OPUS",
      [0x11] = "Hidden FAT12",
      [0x12] = "Compaq diagnostics",
      [0x14] = "Hidden FAT16 <32M",
      [0x16] = "Hidden FAT16",
      [0x17] = "Hidden HPFS/NTFS",
      [0x18] = "AST SmartSleep",
      [0x1b] = "Hidden W95 FAT32",
      [0x1c] = "Hidden W95 FAT32 (LBA)",
      [0x1e] = "Hidden W95 FAT16 (LBA)",
      [0x20] = "Pintos OS kernel",
      [0x21] = "Pintos file system",
      [0x22] = "Pintos scratch",
      [0x23] = "Pintos swap",
      [0x24] = "NEC DOS",
      [0x39] = "Plan 9",
      [0x3c] = "PartitionMagic recovery",
      [0x40] = "Venix 80286",
      [0x41] = "PPC PReP Boot",
      [0x42] = "SFS",
      [0x4d] = "QNX4.x",
      [0x4e] = "QNX4.x 2nd part",
      [0x4f] = "QNX4.x 3rd part",
      [0x50] = "OnTrack DM",
      [0x51] = "OnTrack DM6 Aux1",
      [0x52] = "CP/M",
      [0x53] = "OnTrack DM6 Aux3",
      [0x54] = "OnTrackDM6",
      [0x55] = "EZ-Drive",
      [0x56] = "Golden Bow",
      [0x5c] = "Priam Edisk",
      [0x61] = "SpeedStor",
      [0x63] = "GNU HURD or SysV",
      [0x64] = "Novell Netware 286",
      [0x65] = "Novell Netware 386",
      [0x70] = "DiskSecure Multi-Boot",
      [0x75] = "PC/IX",
      [0x80] = "Old Minix",
      [0x81] = "Minix / old Linux",
      [0x82] = "Linux swap / Solaris",
      [0x83] = "Linux",
      [0x84] = "OS/2 hidden C: drive",
      [0x85] = "Linux extended",
      [0x86] = "NTFS volume set",
      [0x87] = "NTFS volume set",
      [0x88] = "Linux plaintext",
      [0x8e] = "Linux LVM",
      [0x93] = "Amoeba",
      [0x94] = "Amoeba BBT",
      [0x9f] = "BSD/OS",
      [0xa0] = "IBM Thinkpad hibernation",
      [0xa5] = "FreeBSD",
      [0xa6] = "OpenBSD",
      [0xa7] = "NeXTSTEP",
      [0xa8] = "Darwin UFS",
      [0xa9] = "NetBSD",
      [0xab] = "Darwin boot",
      [0xb7] = "BSDI fs",
      [0xb8] = "BSDI swap",
      [0xbb] = "Boot Wizard hidden",
      [0xbe] = "Solaris boot",
      [0xbf] = "Solaris",
      [0xc1] = "DRDOS/sec (FAT-12)",
      [0xc4] = "DRDOS/sec (FAT-16 < 32M)",
      [0xc6] = "DRDOS/sec (FAT-16)",
      [0xc7] = "Syrinx",
      [0xda] = "Non-FS data",
      [0xdb] = "CP/M / CTOS / ...",
      [0xde] = "Dell Utility",
      [0xdf] = "BootIt",
      [0xe1] = "DOS access",
      [0xe3] = "DOS R/O",
      [0xe4] = "SpeedStor",
      [0xeb] = "BeOS fs",
      [0xee] = "EFI GPT",
      [0xef] = "EFI (FAT-12/16/32)",
      [0xf0] = "Linux/PA-RISC boot",
      [0xf1] = "SpeedStor",
      [0xf4] = "SpeedStor",
      [0xf2] = "DOS secondary",
      [0xfd] = "Linux raid autodetect",
      [0xfe] = "LANstep",
      [0xff] = "BBT",
    };

  return type_names[type] != NULL ? type_names[type] : "Unknown";
}

/* Reads sector SECTOR from partition P into BUFFER, which must
   have room for BLOCK_SECTOR_SIZE bytes. */
static void
partition_read (void *p_, block_sector_t sector, void *buffer)
{
  struct partition *p = p_;
  block_read (p->block, p->start + sector, buffer);
}

/* Write sector SECTOR to partition P from BUFFER, which must
   contain BLOCK_SECTOR_SIZE bytes.  Returns after the block has
   acknowledged receiving the data. */
static void
partition_write (void *p_, block_sector_t sector, const void *buffer)
{
  struct partition *p = p_;
  block_write (p->block, p->start + sector, buffer);
}

static struct block_operations partition_operations =
  {
    partition_read,
    partition_write
  };

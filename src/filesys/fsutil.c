#include "filesys/fsutil.h"
#include <debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ustar.h>
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"

/* List files in the root directory. */
void
fsutil_ls (char **argv UNUSED) 
{
  struct dir *dir;
  char name[NAME_MAX + 1];
  
  printf ("Files in the root directory:\n");
  dir = dir_open_root ();
  if (dir == NULL)
    PANIC ("root dir open failed");
  while (dir_readdir (dir, name))
    printf ("%s\n", name);
  dir_close (dir);
  printf ("End of listing.\n");
}

/* Prints the contents of file ARGV[1] to the system console as
   hex and ASCII. */
void
fsutil_cat (char **argv)
{
  const char *file_name = argv[1];
  
  struct file *file;
  char *buffer;

  printf ("Printing '%s' to the console...\n", file_name);
  file = filesys_open (file_name);
  if (file == NULL)
    PANIC ("%s: open failed", file_name);
  buffer = palloc_get_page (PAL_ASSERT);
  for (;;) 
    {
      off_t pos = file_tell (file);
      off_t n = file_read (file, buffer, PGSIZE);
      if (n == 0)
        break;

      hex_dump (pos, buffer, n, true); 
    }
  palloc_free_page (buffer);
  file_close (file);
}

/* Deletes file ARGV[1]. */
void
fsutil_rm (char **argv) 
{
  const char *file_name = argv[1];
  
  printf ("Deleting '%s'...\n", file_name);
  if (!filesys_remove (file_name))
    PANIC ("%s: delete failed\n", file_name);
}

/* Extracts a ustar-format tar archive from the scratch block
   device into the Pintos file system. */
void
fsutil_extract (char **argv UNUSED) 
{
  static block_sector_t sector = 0;

  struct block *src;
  void *header, *data;

  /* Allocate buffers. */
  header = malloc (BLOCK_SECTOR_SIZE);
  data = malloc (BLOCK_SECTOR_SIZE);
  if (header == NULL || data == NULL)
    PANIC ("couldn't allocate buffers");

  /* Open source block device. */
  src = block_get_role (BLOCK_SCRATCH);
  if (src == NULL)
    PANIC ("couldn't open scratch device");

  printf ("Extracting ustar archive from scratch device "
          "into file system...\n");

  for (;;)
    {
      const char *file_name;
      const char *error;
      enum ustar_type type;
      int size;

      /* Read and parse ustar header. */
      block_read (src, sector++, header);
      error = ustar_parse_header (header, &file_name, &type, &size);
      if (error != NULL)
        PANIC ("bad ustar header in sector %"PRDSNu" (%s)", sector - 1, error);

      if (type == USTAR_EOF)
        {
          /* End of archive. */
          break;
        }
      else if (type == USTAR_DIRECTORY)
        printf ("ignoring directory %s\n", file_name);
      else if (type == USTAR_REGULAR)
        {
          struct file *dst;

          printf ("Putting '%s' into the file system...\n", file_name);

          /* Create destination file. */
          if (!filesys_create (file_name, size))
            PANIC ("%s: create failed", file_name);
          dst = filesys_open (file_name);
          if (dst == NULL)
            PANIC ("%s: open failed", file_name);

          /* Do copy. */
          while (size > 0)
            {
              int chunk_size = (size > BLOCK_SECTOR_SIZE
                                ? BLOCK_SECTOR_SIZE
                                : size);
              block_read (src, sector++, data);
              if (file_write (dst, data, chunk_size) != chunk_size)
                PANIC ("%s: write failed with %d bytes unwritten",
                       file_name, size);
              size -= chunk_size;
            }

          /* Finish up. */
          file_close (dst);
        }
    }

  /* Erase the ustar header from the start of the block device,
     so that the extraction operation is idempotent.  We erase
     two blocks because two blocks of zeros are the ustar
     end-of-archive marker. */
  printf ("Erasing ustar archive...\n");
  memset (header, 0, BLOCK_SECTOR_SIZE);
  block_write (src, 0, header);
  block_write (src, 1, header);

  free (data);
  free (header);
}

/* Copies file FILE_NAME from the file system to the scratch
   device, in ustar format.

   The first call to this function will write starting at the
   beginning of the scratch device.  Later calls advance across
   the device.  This position is independent of that used for
   fsutil_extract(), so `extract' should precede all
   `append's. */
void
fsutil_append (char **argv)
{
  static block_sector_t sector = 0;

  const char *file_name = argv[1];
  void *buffer;
  struct file *src;
  struct block *dst;
  off_t size;

  printf ("Appending '%s' to ustar archive on scratch device...\n", file_name);

  /* Allocate buffer. */
  buffer = malloc (BLOCK_SECTOR_SIZE);
  if (buffer == NULL)
    PANIC ("couldn't allocate buffer");

  /* Open source file. */
  src = filesys_open (file_name);
  if (src == NULL)
    PANIC ("%s: open failed", file_name);
  size = file_length (src);

  /* Open target block device. */
  dst = block_get_role (BLOCK_SCRATCH);
  if (dst == NULL)
    PANIC ("couldn't open scratch device");
  
  /* Write ustar header to first sector. */
  if (!ustar_make_header (file_name, USTAR_REGULAR, size, buffer))
    PANIC ("%s: name too long for ustar format", file_name);
  block_write (dst, sector++, buffer);

  /* Do copy. */
  while (size > 0) 
    {
      int chunk_size = size > BLOCK_SECTOR_SIZE ? BLOCK_SECTOR_SIZE : size;
      if (sector >= block_size (dst))
        PANIC ("%s: out of space on scratch device", file_name);
      if (file_read (src, buffer, chunk_size) != chunk_size)
        PANIC ("%s: read failed with %"PROTd" bytes unread", file_name, size);
      memset (buffer + chunk_size, 0, BLOCK_SECTOR_SIZE - chunk_size);
      block_write (dst, sector++, buffer);
      size -= chunk_size;
    }

  /* Write ustar end-of-archive marker, which is two consecutive
     sectors full of zeros.  Don't advance our position past
     them, though, in case we have more files to append. */
  memset (buffer, 0, BLOCK_SECTOR_SIZE);
  block_write (dst, sector, buffer);
  block_write (dst, sector, buffer + 1);

  /* Finish up. */
  file_close (src);
  free (buffer);
}

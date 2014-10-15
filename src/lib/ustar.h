#ifndef __LIB_USTAR_H
#define __LIB_USTAR_H

/* Support for the standard Posix "ustar" format.  See the
   documentation of the "pax" utility in [SUSv3] for the the
   "ustar" format specification. */

#include <stdbool.h>

/* Type of a file entry in an archive.
   The values here are the bytes that appear in the file format.
   Only types of interest to Pintos are listed here. */
enum ustar_type
  {
    USTAR_REGULAR = '0',        /* Ordinary file. */
    USTAR_DIRECTORY = '5',      /* Directory. */
    USTAR_EOF = -1              /* End of archive (not an official value). */
  };

/* Size of a ustar archive header, in bytes. */
#define USTAR_HEADER_SIZE 512

bool ustar_make_header (const char *file_name, enum ustar_type,
                        int size, char header[USTAR_HEADER_SIZE]);
const char *ustar_parse_header (const char header[USTAR_HEADER_SIZE],
                                const char **file_name,
                                enum ustar_type *, int *size);

#endif /* lib/ustar.h */

#include <ustar.h>
#include <limits.h>
#include <packed.h>
#include <stdio.h>
#include <string.h>

/* Header for ustar-format tar archive.  See the documentation of
   the "pax" utility in [SUSv3] for the the "ustar" format
   specification. */
struct ustar_header
  {
    char name[100];             /* File name.  Null-terminated if room. */
    char mode[8];               /* Permissions as octal string. */
    char uid[8];                /* User ID as octal string. */
    char gid[8];                /* Group ID as octal string. */
    char size[12];              /* File size in bytes as octal string. */
    char mtime[12];             /* Modification time in seconds
                                   from Jan 1, 1970, as octal string. */
    char chksum[8];             /* Sum of octets in header as octal string. */
    char typeflag;              /* An enum ustar_type value. */
    char linkname[100];         /* Name of link target.
                                   Null-terminated if room. */
    char magic[6];              /* "ustar\0" */
    char version[2];            /* "00" */
    char uname[32];             /* User name, always null-terminated. */
    char gname[32];             /* Group name, always null-terminated. */
    char devmajor[8];           /* Device major number as octal string. */
    char devminor[8];           /* Device minor number as octal string. */
    char prefix[155];           /* Prefix to file name.
                                   Null-terminated if room. */
    char padding[12];           /* Pad to 512 bytes. */
  }
PACKED;

/* Returns the checksum for the given ustar format HEADER. */
static unsigned int
calculate_chksum (const struct ustar_header *h)
{
  const uint8_t *header = (const uint8_t *) h;
  unsigned int chksum;
  size_t i;

  chksum = 0;
  for (i = 0; i < USTAR_HEADER_SIZE; i++)
    {
      /* The ustar checksum is calculated as if the chksum field
         were all spaces. */
      const size_t chksum_start = offsetof (struct ustar_header, chksum);
      const size_t chksum_end = chksum_start + sizeof h->chksum;
      bool in_chksum_field = i >= chksum_start && i < chksum_end;
      chksum += in_chksum_field ? ' ' : header[i];
    }
  return chksum;
}

/* Drop possibly dangerous prefixes from FILE_NAME and return the
   stripped name.  An archive with file names that start with "/"
   or "../" could cause a naive tar extractor to write to
   arbitrary parts of the file system, not just the destination
   directory.  We don't want to create such archives or be such a
   naive extractor.

   The return value can be a suffix of FILE_NAME or a string
   literal. */
static const char *
strip_antisocial_prefixes (const char *file_name)
{
  while (*file_name == '/'
         || !memcmp (file_name, "./", 2)
         || !memcmp (file_name, "../", 3))
    file_name = strchr (file_name, '/') + 1;
  return *file_name == '\0' || !strcmp (file_name, "..") ? "." : file_name;
}

/* Composes HEADER as a USTAR_HEADER_SIZE (512)-byte archive
   header in ustar format for a SIZE-byte file named FILE_NAME of
   the given TYPE.  The caller is responsible for writing the
   header to a file or device.

   If successful, returns true.  On failure (due to an
   excessively long file name), returns false. */
bool
ustar_make_header (const char *file_name, enum ustar_type type,
                   int size, char header[USTAR_HEADER_SIZE])
{
  struct ustar_header *h = (struct ustar_header *) header;

  ASSERT (sizeof (struct ustar_header) == USTAR_HEADER_SIZE);
  ASSERT (type == USTAR_REGULAR || type == USTAR_DIRECTORY);

  /* Check file name. */
  file_name = strip_antisocial_prefixes (file_name);
  if (strlen (file_name) > 99)
    {
      printf ("%s: file name too long\n", file_name);
      return false;
    }

  /* Fill in header except for final checksum. */
  memset (h, 0, sizeof *h);
  strlcpy (h->name, file_name, sizeof h->name);
  snprintf (h->mode, sizeof h->mode, "%07o",
            type == USTAR_REGULAR ? 0644 : 0755);
  strlcpy (h->uid, "0000000", sizeof h->uid);
  strlcpy (h->gid, "0000000", sizeof h->gid);
  snprintf (h->size, sizeof h->size, "%011o", size);
  snprintf (h->mtime, sizeof h->size, "%011o", 1136102400);
  h->typeflag = type;
  strlcpy (h->magic, "ustar", sizeof h->magic);
  h->version[0] = h->version[1] = '0';
  strlcpy (h->gname, "root", sizeof h->gname);
  strlcpy (h->uname, "root", sizeof h->uname);

  /* Compute and fill in final checksum. */
  snprintf (h->chksum, sizeof h->chksum, "%07o", calculate_chksum (h));

  return true;
}

/* Parses a SIZE-byte octal field in S in the format used by
   ustar format.  If successful, stores the field's value in
   *VALUE and returns true; on failure, returns false.

   ustar octal fields consist of a sequence of octal digits
   terminated by a space or a null byte.  The ustar specification
   seems ambiguous as to whether these fields must be padded on
   the left with '0's, so we accept any field that fits in the
   available space, regardless of whether it fills the space. */
static bool
parse_octal_field (const char *s, size_t size, unsigned long int *value)
{
  size_t ofs;

  *value = 0;
  for (ofs = 0; ofs < size; ofs++)
    {
      char c = s[ofs];
      if (c >= '0' && c <= '7')
        {
          if (*value > ULONG_MAX / 8)
            {
              /* Overflow. */
              return false;
            }
          *value = c - '0' + *value * 8;
        }
      else if (c == ' ' || c == '\0')
        {
          /* End of field, but disallow completely empty
             fields. */
          return ofs > 0;
        }
      else
        {
          /* Bad character. */
          return false;
        }
    }

  /* Field did not end in space or null byte. */
  return false;
}

/* Returns true if the CNT bytes starting at BLOCK are all zero,
   false otherwise. */
static bool
is_all_zeros (const char *block, size_t cnt)
{
  while (cnt-- > 0)
    if (*block++ != 0)
      return false;
  return true;
}

/* Parses HEADER as a ustar-format archive header for a regular
   file or directory.  If successful, stores the archived file's
   name in *FILE_NAME (as a pointer into HEADER or a string
   literal), its type in *TYPE, and its size in bytes in *SIZE,
   and returns a null pointer.  On failure, returns a
   human-readable error message. */
const char *
ustar_parse_header (const char header[USTAR_HEADER_SIZE],
                    const char **file_name, enum ustar_type *type, int *size)
{
  const struct ustar_header *h = (const struct ustar_header *) header;
  unsigned long int chksum, size_ul;

  ASSERT (sizeof (struct ustar_header) == USTAR_HEADER_SIZE);

  /* Detect end of archive. */
  if (is_all_zeros (header, USTAR_HEADER_SIZE))
    {
      *file_name = NULL;
      *type = USTAR_EOF;
      *size = 0;
      return NULL;
    }

  /* Validate ustar header. */
  if (memcmp (h->magic, "ustar", 6))
    return "not a ustar archive";
  else if (h->version[0] != '0' || h->version[1] != '0')
    return "invalid ustar version";
  else if (!parse_octal_field (h->chksum, sizeof h->chksum, &chksum))
    return "corrupt chksum field";
  else if (chksum != calculate_chksum (h))
    return "checksum mismatch";
  else if (h->name[sizeof h->name - 1] != '\0' || h->prefix[0] != '\0')
    return "file name too long";
  else if (h->typeflag != USTAR_REGULAR && h->typeflag != USTAR_DIRECTORY)
    return "unimplemented file type";
  if (h->typeflag == USTAR_REGULAR)
    {
      if (!parse_octal_field (h->size, sizeof h->size, &size_ul))
        return "corrupt file size field";
      else if (size_ul > INT_MAX)
        return "file too large";
    }
  else
    size_ul = 0;

  /* Success. */
  *file_name = strip_antisocial_prefixes (h->name);
  *type = h->typeflag;
  *size = size_ul;
  return NULL;
}


#ifndef THREADS_PTE_H
#define THREADS_PTE_H

#include "threads/vaddr.h"

/* Functions and macros for working with x86 hardware page
   tables.

   See vaddr.h for more generic functions and macros for virtual
   addresses.
   
   Virtual addresses are structured as follows:

    31                  22 21                  12 11                   0
   +----------------------+----------------------+----------------------+
   | Page Directory Index |   Page Table Index   |    Page Offset       |
   +----------------------+----------------------+----------------------+
*/

/* Page table index (bits 12:21). */
#define	PTSHIFT PGBITS		           /* First page table bit. */
#define PTBITS  10                         /* Number of page table bits. */
#define PTSPAN  (1 << PTBITS << PGBITS)    /* Bytes covered by a page table. */
#define PTMASK  BITMASK(PTSHIFT, PTBITS)   /* Page table bits (12:21). */

/* Page directory index (bits 22:31). */
#define PDSHIFT (PTSHIFT + PTBITS)         /* First page directory bit. */
#define PDBITS  10                         /* Number of page dir bits. */
#define PDMASK  BITMASK(PDSHIFT, PDBITS)   /* Page directory bits (22:31). */

/* Obtains page table index from a virtual address. */
static inline unsigned pt_no (const void *va) {
  return ((uintptr_t) va & PTMASK) >> PTSHIFT;
}

/* Obtains page directory index from a virtual address. */
static inline uintptr_t pd_no (const void *va) {
  return (uintptr_t) va >> PDSHIFT;
}

/* Page directory and page table entries.

   For more information see the section on page tables in the
   Pintos reference guide chapter, or [IA32-v3a] 3.7.6
   "Page-Directory and Page-Table Entries".

   PDEs and PTEs share a common format:

   31                                 12 11                     0
   +------------------------------------+------------------------+
   |         Physical Address           |         Flags          |
   +------------------------------------+------------------------+

   In a PDE, the physical address points to a page table.
   In a PTE, the physical address points to a data or code page.
   The important flags are listed below.
   When a PDE or PTE is not "present", the other flags are
   ignored.
   A PDE or PTE that is initialized to 0 will be interpreted as
   "not present", which is just fine. */
#define PTE_FLAGS 0x00000fff    /* Flag bits. */
#define PTE_ADDR  0xfffff000    /* Address bits. */
#define PTE_AVL   0x00000e00    /* Bits available for OS use. */
#define PTE_P 0x1               /* 1=present, 0=not present. */
#define PTE_W 0x2               /* 1=read/write, 0=read-only. */
#define PTE_U 0x4               /* 1=user/kernel, 0=kernel only. */
#define PTE_A 0x20              /* 1=accessed, 0=not acccessed. */
#define PTE_D 0x40              /* 1=dirty, 0=not dirty (PTEs only). */

/* Returns a PDE that points to page table PT. */
static inline uint32_t pde_create (uint32_t *pt) {
  ASSERT (pg_ofs (pt) == 0);
  return vtop (pt) | PTE_U | PTE_P | PTE_W;
}

/* Returns a pointer to the page table that page directory entry
   PDE, which must "present", points to. */
static inline uint32_t *pde_get_pt (uint32_t pde) {
  ASSERT (pde & PTE_P);
  return ptov (pde & PTE_ADDR);
}

/* Returns a PTE that points to PAGE.
   The PTE's page is readable.
   If WRITABLE is true then it will be writable as well.
   The page will be usable only by ring 0 code (the kernel). */
static inline uint32_t pte_create_kernel (void *page, bool writable) {
  ASSERT (pg_ofs (page) == 0);
  return vtop (page) | PTE_P | (writable ? PTE_W : 0);
}

/* Returns a PTE that points to PAGE.
   The PTE's page is readable.
   If WRITABLE is true then it will be writable as well.
   The page will be usable by both user and kernel code. */
static inline uint32_t pte_create_user (void *page, bool writable) {
  return pte_create_kernel (page, writable) | PTE_U;
}

/* Returns a pointer to the page that page table entry PTE points
   to. */
static inline void *pte_get_page (uint32_t pte) {
  return ptov (pte & PTE_ADDR);
}

#endif /* threads/pte.h */


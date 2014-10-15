#ifndef USERPROG_TSS_H
#define USERPROG_TSS_H

#include <stdint.h>

struct tss;
void tss_init (void);
struct tss *tss_get (void);
void tss_update (void);

#endif /* userprog/tss.h */

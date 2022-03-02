#ifndef __UUID_H__
#define __UUID_H__

#include "common_header.h"
#include "copyrt.h"

#undef uuid_t
typedef struct {
  uint32 time_low;
  uint16 time_mid;
  uint16 time_hi_and_version;
  uint8 clock_seq_hi_and_reserved;
  uint8 clock_seq_low;
  byte node[6];
} uuid_t;

/* uuid_create_md5_from_name -- create a version 3 (MD5) UUID using a
   "name" from a "name space" */
void uuid_create_md5_from_name(
    uuid_t *uuid, /* resulting UUID */
    void *name,   /* the name from which to generate a UUID */
    int namelen   /* the length of the name */
);

#endif

#include "uuid.h"
#include "copyrt.h"
#include "md5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ntohl(x)                                                                                                       \
    ((((x)&0x000000ffU) << 24) | (((x)&0x0000ff00U) << 8) | (((x)&0x00ff0000U) >> 8) | (((x)&0xff000000U) >> 24))

#define ntohs(x) ((((x)&0x00ffU) << 8) | (((x)&0xff00U) >> 8))

#define htonl(x) ntohl(x)

#define htons(x) ntohs(x)

/* various forward declarations */
static void format_uuid_v3or5(uuid_t* uuid, unsigned char hash[16], int v);

/* uuid_create_md5_from_name -- create a version 3 (MD5) UUID using a
 "name" from a "name space" */
void uuid_create_md5_from_name(uuid_t* uuid, void* name, int namelen)
{
    MD5_CTX c;
    unsigned char hash[16];

    MD5Init(&c);
    MD5Update(&c, name, namelen);
    MD5Final(hash, &c);

    /* the hash is in network byte order at this point */
    format_uuid_v3or5(uuid, hash, 3);
}

/* format_uuid_v3or5 -- make a UUID from a (pseudo)random 128-bit
   number */
void format_uuid_v3or5(uuid_t* uuid, unsigned char hash[16], int v)
{
    /* convert UUID to local byte order */
    memcpy(uuid, hash, sizeof *uuid);
    uuid->time_low = ntohl(uuid->time_low);
    uuid->time_mid = ntohs(uuid->time_mid);
    uuid->time_hi_and_version = ntohs(uuid->time_hi_and_version);

    /* put in the variant and version bits */
    uuid->time_hi_and_version &= 0x0FFF;
    uuid->time_hi_and_version |= (v << 12);
    uuid->clock_seq_hi_and_reserved &= 0x3F;
    uuid->clock_seq_hi_and_reserved |= 0x80;
}

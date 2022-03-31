#ifndef __NBIOT_RESPONSE_H__
#define __NBIOT_RESPONSE_H__

#include "nbiotModem.h"
#include "common_header.h"

#define DFOTA_IP "1.233.95.235"
#define DFOTA_PORT "18099"

unsigned char parse_cereg(char *p, ModemContext_t *modemPtr);
BOOL parseQLWULDATAEX(const char *pHead, ModemContext_t *modemPtr);
BOOL parseQGMR(const char *pHead, Modem_t *modemPtr);

#endif

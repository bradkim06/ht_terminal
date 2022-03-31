#ifndef __FOTA_H__
#define __FOTA_H__

extern int fotaStatus;
extern char rxBuf[512];
extern int rxLen;

static int checkFlashBusy();
static void eraseCode(unsigned long addr, int option);
static void writeCode(unsigned long addr);
static void fotaPrint(char *str);
static void modemSend(char *str);
static void fotaModemPrint(char *p, int len);
static void fotaSend(char *str);
static void fotaRecv();

void startFota();

#endif

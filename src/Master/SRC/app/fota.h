#ifndef __FOTA_H__
#define __FOTA_H__

#define HEADER_LEN 2
#define MAX_DATA_LEN 1024
#define CHECKSUM_LEN 2
#define RX_MAX_LEN HEADER_LEN + MAX_DATA_LEN + CHECKSUM_LEN

#define FOTA_IP_ADDR_LEN 16
#define FOTA_FW_VER_LEN 4
typedef struct {
	char ip[FOTA_IP_ADDR_LEN + 1];
	unsigned int port;
	char version[FOTA_FW_VER_LEN + 1];
} FotaReqInfo_t;

typedef struct {
	char Req;
	char Status;
	FotaReqInfo_t Info;
} FotaStatus_t;

extern FotaStatus_t Fota;
extern char rxBuf[RX_MAX_LEN];
extern int rxLen;

static int checkFlashBusy();
static void modemRxClear();
static void eraseCode(unsigned long addr, int option);
static void writeCode(unsigned long addr, volatile unsigned char data[], int size);
static void modemSend(char *str);
static void fotaSend(char *str);
static void fotaRecv();
static void send(int option);
#ifdef FOTA_DEBUG
static void fotaModemPrint(char *p);
static void fotaPrint(char *str);
#endif

void startFota();

#endif

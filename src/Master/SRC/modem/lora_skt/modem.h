#ifndef __MODEM_H__
#define __MODEM_H__

#define MODEM_MAX_AT_CMD_LEN MODEM_TX_BUF_LEN
#define MODEM_MAX_TRY_COUNT 2

typedef enum { LORA_SEUDO_JOIN = 0, LORA_REAL_JOIN = 1 } JoinMode_t;

typedef struct {
	int rssi;
	int snr;
} SigQuality_t;

typedef struct {
	uchar devEui[8];
	uchar appEui[8];
	uchar appKey[16];
	uchar userAppEui[8];
	uchar userAppKey[16];
	JoinMode_t lastUserJoin;
	SigQuality_t sigQuality;
} Modem_t;

extern Modem_t modem;

void MODEM_turnOff();
void MODEM_turnOn();
void MODEM_reset();
void MODEM_read();
void MODEM_write(char *pCmd);

void MODEM_initialization();
void MODEM_enable();
void MODEM_disable();
void MODEM_open(int process);
void MODEM_stop(int resultCode);
void MODEM_close();
void MODEM_timeout();

void MODEM_handler();

BOOL MODEM_setUserNwk(uchar *aeui, uchar *akey, JoinMode_t joinMode);
void MODEM_copyAppKey(uchar *akey);
void MODEM_copyAppEui(uchar *aeui);
void MODEM_copyDevEui(uchar *deui);
int MODEM_getLastRssi();
int MODEM_getLastSNR();

int MODEM_getStateOrResult();
int MODEM_getAccessState();
void MODEM_copyLastAccessTime(uint8 *p);

#endif

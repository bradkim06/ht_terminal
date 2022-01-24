#ifndef __MODEM_H__
#define __MODEM_H__

typedef struct {
	int joined;
	int failCount;
	uint8 lastRSSI;
	int8 lastSNR;
	int8 lastError;
	uint8 timeoutCount;
	uint8 step;
	uchar devEui[8];
	uchar appEui[8];
	Date_t lastAccessTime;
} Modem_t;

typedef struct {
	uint16 alive : 1, gotDevEui : 1, gotAppEui : 1, gotRssi : 1, gotSnr : 1, gotTime : 1,
		acked : 1, busy : 1, timeoutOccurred : 1, reserved : 7;
} ModemFlag_t;

// Modem access step
#define MODEM_STEP_IDLE 0
#define MODEM_STEP_CHECK_ALIVE 1
#define MODEM_STEP_GET_APP_EUI 2
#define MODEM_STEP_GET_DEV_EUI 3
#define MODEM_STEP_WAIT_JOIN 4
#define MODEM_STEP_WAIT_ACK_DATA 5
#define MODEM_STEP_GET_RSSI 6
#define MODEM_STEP_GET_SNR 7
#define MODEM_STEP_GET_TIME 8

#define MODEM_TX_RETRY_COUNT 16 // 실제는 8번이나, RX1/RX2 timeout을 합산하여 16으로 함.

typedef struct {
	uint8 rfu : 6, ver : 2;
	uint8 msgType;
	uint8 len;
	uint8 payload[0x100];
} LoraDevMgmtMsg_t;

// application message for device management
#define DEV_RESET 0x80
#define REP_PER_CHANGE 0x81
#define REP_IMMEDIATE 0x82

void MODEM_write(char *cmd);

void MODEM_turnOff();
void MODEM_turnOn();
void MODEM_reset();
void MODEM_initialization();
void MODEM_enable();
void MODEM_disable();
void MODEM_timeout();
void MODEM_open(int process);
void MODEM_close();
void MODEM_sendData(uint8 *data, int len);
void MODEM_getRSSI();
void MODEM_getTime();
void MODEM_handler();
void MODEM_checkAlive();
void MODEM_read();
void MODEM_wakeup();
void MODEM_waitJoin();

void SEM_MODEM_parse(char *pHead, int len);
void SOLUM_MODEM_parse(char *pHead, int len);

void MODEM_copyAppEui(uchar *aeui);
void MODEM_copyDevEui(uchar *deui);
int MODEM_getLastRssi();
int MODEM_getLastSNR();
int MODEM_inReportProcess();
int MODEM_getStateOrResult();

void MODEM_copyLastAccessTime(uint8 *p);
void MODEM_deleteLastAccessTime();
int MODEM_getAccessState();

void MODEM_sendDataReport();
void MODEM_sendJoin();
void MODEM_sktReport();
void MODEM_sktAttach();

void parse_appEui(char *p);
void parse_devEui(char *p);

#endif

#ifndef __MODEM_H__
#define __MODEM_H__

#include "uart.h"
#include "common_header.h"

// LG platform 품질관리 서버
#define LG_QA_SERVER_IP "10.120.182.10"
#define LG_QA_SERVER_PORT 50000
#define LG_ERR_SERVER_IP "10.120.182.11"
#define LG_ERR_SERVER_PORT 50001

#define LG_DEFAULT_FOTA_SERVER_IP "106.103.250.108" // "106.103.233.155"
#define LG_DEFAULT_FOTA_SERVER_PORT 5783
#define LG_DEFAULT_FOTA_DAY_INTERVAL 15

#define LG_QA_HOUR_INTERVAL 24
#define LG_CERTIFY_HOUR_INTERVAL 18
// #define LG_PUASED_RETRY_HOUR_INTERVAL   48
#define LG_PUASED_RETRY_HOUR_INTERVAL 24

// NB-IoT 총 재시도 횟수 (마지막 1회는 H/W reset, 그 외 재시도는 S/W reset)
#define NBIOT_BIP_RETRY 2
#define NBIOT_ATTACH_RETRY 1
#define NBIOT_ONEM2M_RETRY 1

// LG platform 연동시 생성하는 데이터 길이 정의
#define LEN_MODEM_IMEI_IMSI 15
#define LEN_MODEM_BS_PARAM 100
#define LEN_MODEM_EP_NAME 25
#define LEN_MODEM_ICCID 19
#define LEN_MODEM_UUID 36
#define LEN_MODEM_CTN 11
#define LEN_MODEM_SERVICE_CODE 4

#define LEN_MAX_AT_DATA MODEM_TX_BUF_LEN
#define LEN_MAX_DL_DATA 0x20

// MCC in IMSI
#define IMSI_MCC_KOREA "450"

// MNC in IMSO
#define IMSI_MCC_MNC_SKT IMSI_MCC_KOREA "05"
#define IMSI_MCC_MNC_KT_1 IMSI_MCC_KOREA "04"
#define IMSI_MCC_MNC_KT_2 IMSI_MCC_KOREA "08"
#define IMSI_MCC_MNC_UPLUS IMSI_MCC_KOREA "06"

// modem type definition
typedef enum {
	MODEM_TYPE_UPLUS = 0,
	MODEM_TYPE_KT = 1,
	MODEM_TYPE_UNKNOWN = 99,
} ModemType_t;

// modem type definition
typedef enum {
	MODEM_NW_STATUS_NORMAL = 0,
	MODEM_NW_STATUS_PAUSED = 1,
	MODEM_NW_STATUS_REJECTED = 2,
} ModemNwStatus_t;

typedef struct {
	int lastRSSI;
	int cid; // Physical Cell ID
	int rsrp;
	int rsrq;
	int snr;
	int txPower;
	uint32 cgi; // Serving Cell ID
} ModemQuality_t;

typedef struct {
	ModemQuality_t modemQuality;
	char imeiStr[LEN_MODEM_IMEI_IMSI + 1];
	char imsiStr[LEN_MODEM_IMEI_IMSI + 1];
	char ctnStr[LEN_MODEM_CTN + 1];
	char iccidStr[LEN_MODEM_ICCID + 1];
	char epName[LEN_MODEM_EP_NAME + 1];
	char bsParam[LEN_MODEM_BS_PARAM + 1];
	Date_t lastUpdateTime;
	Date_t lastCertifyTime;
	Date_t lastAccessTime;
	Date_t lastAttachTime;
	ModemNwStatus_t lastAttachStatus;
	ModemType_t modemType;
} Modem_t;

void MODEM_turnOff();
void MODEM_turnOn();
void MODEM_initialization();
void MODEM_enable();
void MODEM_disable();
void MODEM_write(char *cmd);
void MODEM_read();

void MODEM_open(int process);
void MODEM_close();
void MODEM_timeout();
void MODEM_stop(int resultCode);
void MODEM_handler();

/**
 * @brief CSE-ID generate function (defined in nbiotProcess.c)
 *
 * @param serviceCode  Service code string data
 * @param ctnStr       USIM ctn string data
 * @param iccidStr     USIM ICCID string data
 * @param buffer       Buffer for getting data.
 * @param size         'buffer' size. If '0', CSE-ID will be set into modem.epName
 *
 * @return BOOL        TRUE is success, FALSE is fail
 */
BOOL MODEM_generateCseID(const char *serviceCode, const char *ctnStr, const char *iccidStr,
			 char *buffer, int size);

void MODEM_copyImei(uchar *imei);
void MODEM_copyImsi(uchar *imsi);
void MODEM_copyIccid(uchar *iccid);
void MODEM_copyCtn(uchar *ctn);
void MODEM_copyCseId(uchar *cseid);
int MODEM_getLastRssi();
int MODEM_getLastRsrp();
int MODEM_getLastRsrq();
int MODEM_getLastSNR();

int MODEM_getStateOrResult();

void MODEM_copyLastAccessTime(uint8 *p);
int MODEM_getAccessState();

#endif

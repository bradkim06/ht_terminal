#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include "common_header.h"

#define PROTOCOL_VERSION_A1 0xA1
#define PROTOCOL_VERSION_A2 0xA2
#define PROTOCOL_VERSION_A3 0xA3

#if 0
#define NBIOT_JOIN 0x30
#define NBIOT_ACK 0x50
#define NBIOT_INTERVAL_REQ 0x50
#define NBIOT_DATA_REPORT 0x70
#else
#define NBIOT_DATA_REPORT 0x70
#define NBIOT_INTERVAL_REQ 0x50
#define NBIOT_CHANGE_R_RANGE 0x20
#define NBIOT_ACK 0x10
#endif

#define NBIOT_QA_MSG_VER 4
#define NBIOT_ERR_MSG_VER 1

#define LEN_NBIOT_CTN 6

#define LEN_NBIOT_QA_BATT 3
#define LEN_NBIOT_QA_CGI 4
#define LEN_NBIOT_QA_RSRP 2
#define LEN_NBIOT_QA_SINR 2
#define LEN_NBIOT_QA_MODEL TERM_MODEL_STRING_LEN + 1
#define LEN_NBIOT_QA_FW_VER 20
#define LEN_NBIOT_QA_TX_POWER 2
// 본래 위치 좌표 및 Neighbor CELL ID 데이터를 넣어야 하지만
// NB-IoT 자사 제품에서는 지원하지 않는 기능이므로 reserved 처리.
#define LEN_NBIOT_QA_PORT_INFO 3
#define LEN_NBIOT_QA_RESERVED 3

#define NBIOT_QA_BATT_PERCENT 0
#define NBIOT_QA_BATT_VOLTAGE 1
#define NBIOT_QA_BATT_NOTUSE 2

#define LEN_NBIOT_ERR_S_TIME 6
#define LEN_NBIOT_ERR_P_TIME 2
#define LEN_NBIOT_ERR_P_PAYLOAD 2
#define LEN_NBIOT_ERR_UL_DL_CNT 2
#define LEN_NBIOT_ERR_E_LENGTH 2
#define LEN_NBIOT_ERR_E_LOG 7
#define CNT_NBIOT_ERR_E_LOG 4

#define NBIOT_UE_TYPE_WORK_PERIOD 1
#define NBIOT_UE_TYPE_WORK_EVENT 2
#define NBIOT_UE_TYPE_WORK_MIXED 3
#define NBIOT_UE_TYPE_MODE_PF_FIXED 1
#define NBIOT_UE_TYPE_MODE_PF_MOVED 2
#define NBIOT_UE_TYPE_MODE_NPF_FIXED 3
#define NBIOT_UE_TYPE_MODE_NPF_MOVED 4

#define NBIOT_ERR_CURRENT_UE_TYPE ((NBIOT_UE_TYPE_MODE_NPF_FIXED << 4) | NBIOT_UE_TYPE_WORK_MIXED)

#define LEN_MAX_NBIOT_DATA 128
#define LEN_NBIOT_ACK_DATA 26

typedef struct {
	uchar msgVer;
	uchar ctn[LEN_NBIOT_CTN];
	uchar sendTime[LEN_NBIOT_ERR_S_TIME];
	uchar ueType;
	uchar periodLength;
	uchar periodTime[LEN_NBIOT_ERR_P_TIME];
	uchar periodPayload[LEN_NBIOT_ERR_P_PAYLOAD];
	uchar pfUlCnt[LEN_NBIOT_ERR_UL_DL_CNT];
	uchar pfDlCnt[LEN_NBIOT_ERR_UL_DL_CNT];
	uchar udpUlCnt[LEN_NBIOT_ERR_UL_DL_CNT];
	uchar udpDlCnt[LEN_NBIOT_ERR_UL_DL_CNT];
	uchar tcpUlCnt[LEN_NBIOT_ERR_UL_DL_CNT];
	uchar tcpDlCnt[LEN_NBIOT_ERR_UL_DL_CNT];
	uchar npFlag;
	uchar errLength[LEN_NBIOT_ERR_E_LENGTH];
} NbiotErrorReportToLg_t;

typedef struct {
	uchar msgVer;
	uchar ctn[LEN_NBIOT_CTN];
	uchar batt[LEN_NBIOT_QA_BATT];
	uchar cgi[LEN_NBIOT_QA_CGI];
	uchar rsrp[LEN_NBIOT_QA_RSRP];
	uchar sinr[LEN_NBIOT_QA_SINR];
	uchar model[LEN_NBIOT_QA_MODEL];
	uchar fwVer[LEN_NBIOT_QA_FW_VER];
	uchar txPower[LEN_NBIOT_QA_TX_POWER];
	uchar location;
	uchar neighborCell;
	uchar ueInfo;
	uchar portInfo[LEN_NBIOT_QA_PORT_INFO];
	uchar reserved[LEN_NBIOT_QA_RESERVED];
} NbiotQaReportToLg_t;

typedef struct {
	uchar imei[8];
	uchar imsi[8];
} MobileId_t;

typedef struct {
	uchar rssi;
	uchar ber;
	uchar cid[2];
	uchar rsrp[2];
	uchar rsrq[2];
	uchar snr[2];
} NbiotRadioQuality_t;

typedef struct {
	uchar termSerial[5];
	uchar fwVer[2];
	uchar termBatt;
} NbiotDeviceInfo_t;

typedef struct {
	uchar meterSerial[4];
	uchar meterType;
	uchar meterCaliber_dp;
	uchar meterStatus;
} NbiotMeterInfo_t;

typedef struct {
	uchar mi;
	uchar ri;
} NbiotMeteringParam_t;

typedef struct {
	uchar year;
	uchar mon;
	uchar day;
	uchar hour;
	uchar min;
	uchar sec;
} NbiotTimeStamp_t;

typedef struct {
	uchar interval;
	uchar numData;
	uchar refValuePos;
	uchar refValue[4];
	uchar valueDiff[MAX_NUM_STORED_DATA][2];
} NbiotMeterData_t;

typedef struct {
	uchar resetCause;
	uchar resetCount[2];
} NbiotReset_t;

#if 0 // sholee
typedef struct {
	uchar protocol; // 1 -  1
	uchar len; // 1 -  2
	uchar mtype; // 1 -  3
	MobileId_t id; // 16 - 19
	NbiotRadioQuality_t radio; // 10 - 29
	NbiotDeviceInfo_t term; // 8  - 37
	NbiotMeterInfo_t meter; // 7  - 44
	NbiotMeteringParam_t mp; // 2  - 46
	NbiotTimeStamp_t meterTime; // 6  - 52
	uchar data[4]; // 4  - 56
	NbiotReset_t reset; // 3  - 59
	uchar checksum; // 1  - 60
} NbiotJoin_t;

typedef struct {
	uchar protocol; // 1 -  1
	uchar len; // 1 -  2
	uchar mtype; // 1 -  3
	MobileId_t id; // 16 - 19
	NbiotTimeStamp_t currTime; // 6  - 25
	uchar resetCommand; // 1  - 26
	uchar mi; // 1  - 27
	uchar ri; // 1  - 28
	uchar checksum; // 1  - 29
} NbiotHitecAck_t;
#endif

typedef struct {
	uchar protocol; // 1 -  1
	uchar len; // 1 -  2
	uchar mtype; // 1 -  3
	uchar mi; // 1  - 4
	uchar ri; // 1  - 5
	uchar checksum; // 1  - 6
} NbiotIntervalReq_t;

typedef struct {
	uchar protocol; // 1 -  1
	uchar len; // 1 -  2
	uchar mtype; // 1 -  3
	uchar range; // 1  - 4
	uchar checksum; // 1  - 5
} NbiotRepRange_t;

typedef struct {
	uchar protocol; // 1 -  1
	uchar len; // 1 -  2
	uchar mtype; // 1 -  3
	uchar checksum; // 1  - 5
} NbiotAck_t;

typedef struct {
	uchar protocol; // 1 -  1
	uchar len; // 1 -  2
	uchar mtype; // 1 -  3
	MobileId_t id; // 16 - 19
	NbiotRadioQuality_t radio; // 10 - 29
	NbiotDeviceInfo_t term; // 8  - 37
	NbiotMeterInfo_t meter; // 7  - 44
	NbiotMeteringParam_t mp; // 2  - 46
	NbiotTimeStamp_t meterTime; // 6  - 52
	NbiotMeterData_t data; // 7 + 2*nData --> 61 ~ 107
	uchar checksum; // 1  - 108
} NbiotDataReport_t;

int MODEM_errData(uchar *buf, int pfUlCnt, int pfDlCnt, int udpUlCnt, int udpDlCnt);
int MODEM_qaData(uchar *buf);
int MODEM_sendJoin(uchar *buf);
int MODEM_sendCurrentData(uchar *buf);
int MODEM_sendPeriodicData(uchar *buf);
int MODEM_recvAck(char *p, int len);
int MODEM_recvResetReq(uchar *buf, int len);
int MODEM_recvIntervalReq(uchar *buf, int len);
int MODEM_checkDlMessage(uchar *buf, int len);

#endif

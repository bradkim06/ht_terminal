#ifndef _MESSAGE_HEADER_
#define _MESSAGE_HEADER_

#include "common_header.h"
#include "meter.h"
#include "modem.h"

#if defined(AUX_REPEATER)
#include "slaveAccess.h"
#endif

#define DEVICE_CODE_LORA 0x05
#define DEVICE_CODE_NB 0x06
#define DEVICE_CODE_LORA_RF 0x07
#define DEVICE_CODE_SMART_PHONE 0xFA

//=======================================
// NFC Message Type
//=======================================
// SMART Phone -> Node
#define NODE_CONF_REQ 0x01
#define LORA_CONF_SET 0x02
#define NB_CONF_SET 0x03
#define METER_REQ 0x04
#define SERVER_CONNECT_REQ 0x05
#define BD_CONTROL_REQ 0x06
#define PERIOD_METER_REQ 0x07
// #define PERIOD_METER_ACK        0x08 // reserved
#define PULSE_METER_VALUE_SET 0x09
#define DEVICE_INFO_REQ 0x0A
#define SLAVE_CHECK_REQ 0x0B
#define NB_ID_REQ 0x0C
#define NB_ID_SET 0x0D
#define LORA_APP_EUI_KEY_REQ 0x11
#define LORA_APP_EUI_KEY_SET 0x12
#define FLASH_DATE_LIST_REQ 0x13
#define FLASH_DATA_REQ 0x14
#define FW_UPDATE_REQ 0x15

#define METER_ADJUST_REQ 0x41

// Node -> SMART Phone
#define LORA_CONF_REPORT 0x51
#define NB_CONF_REPORT 0x52
#define METER_REPORT 0x53
#define SERVER_CONNECT_REPORT 0x54
#define BD_CONTROL_ACK 0x55
#define FW_VER_REPORT 0x56
#define PERIOD_METER_REPORT 0x57
#define PULSE_METER_VALUE_ACK 0x58
#define DEVICE_INFO_REPORT 0x59
#define SLAVE_CHECK_REPORT 0x5A
#define NB_ID_REPORT 0x5B
#define LORA_APP_EUI_KEY_REPORT 0x61
#define FLASH_DATE_LIST_REPORT 0x63
#define FLASH_DATA_REPORT 0x64
#define FW_UPDATE_REPORT 0x65

#define METER_ADJUST_RESP 0x91
#define METER_ADJUST_ERROR 0x92

#define REPORT_TYPE_REQ 0
#define REPORT_TYPE_SET 1

#define NFC_PROTOCOL_VER_1 0
#define NFC_PROTOCOL_VER_2 1
#define NFC_PROTOCOL_VER_3 2
#define NFC_PROTOCOL_VER_4 3

#define MSG_OFFSET_LEN 2 //Device Code(1) + msgLen(1)
#define MSG_TYPE_POSITION 10

#define CONFIG_ERR_NONE 0
#define CONFIG_ERR_SERIAL_NUM 1
#define CONFIG_ERR_METERING_PARAM 2
#define CONFIG_ERR_METER_TYPE 3
#define CONFIG_ERR_METER_PORT 4
#define CONFIG_ERR_MODEM_FAIL 2

#define MAX_METER_DATA_PER_REPORT 4

#define REPORT_MODE_NOT_USE 0
#define REPORT_MODE_SERVER 1
#define REPORT_MODE_TERMINAL 2

#define SLEEP_MODE_NOT_USE 0
#define SLEEP_MODE_SLEEP 1
#define SLEEP_MODE_NORMAL 2

#define PERIOD_MODE_NO_CHANGE 0
#define PERIOD_MODE_USE 1
#define PERIOD_MODE_NO_USE 2

#define DEBUG_MODE_NO_CHANGE 0
#define DEBUG_JATG_MODE_USE 1
#define DEBUG_METER_MODE_USE 2
#define DEBUG_MODE_NO_USE 3

typedef struct {
	uint8 deviceCode;
	uint8 len;
	uint8 device_id[8];
} NfcHeader_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
} NfcMsg_t;

typedef struct {
	uint8 meterType;
	uint8 meterPort;
} NfcMeterInfo_t;

typedef struct {
	uint8 panId[2];
	uint8 nwkAddr[4];
	uint8 slaveId;
} NfcAmiInfo_t;

//Recv
// For Terminal message format (protocol version 1) Not Use Since U316
/* typedef struct { */
/* 	NfcHeader_t header; */
/* 	uint8 mtype; */
/* 	uint8 mversion; */
/* 	uint8 serialNum[SERIAL_NUM_LEN]; */
/* 	uint8 sleepMode; */
/* 	uint8 year[2]; */
/* 	uint8 mon; */
/* 	uint8 day; */
/* 	uint8 hour; */
/* 	uint8 min; */
/* 	uint8 sec; */
/* 	uint8 meterInterval; */
/* 	uint8 reportInterval; */
/* 	uint8 messageFrame; */
/* #if NBIOT_DEVICE */
/* 	uint8 serverIp[4]; */
/* 	uint8 serverPort[2]; */
/* #endif */
/* 	uint8 meterNum; */
/* 	NfcMeterInfo_t meterInfo[3]; */
/* } NfcConfSetV1_t; */

// For Repeator message format (protocol version 2) Not Use
/* typedef struct { */
/* 	NfcHeader_t header; */
/* 	uint8 mtype; */
/* 	uint8 mversion; */
/* 	uint8 serialNum[SERIAL_NUM_LEN]; */
/* 	uint8 sleepMode; */
/* 	uint8 dateTime[4]; */
/* 	uint8 meterInterval; */
/* 	uint8 reportInterval; */
/* 	uint8 messageFrame; */
/* #if NBIOT_DEVICE */
/* 	uint8 serverIp[4]; */
/* 	uint8 serverPort[2]; */
/* #endif */
/* 	NfcAmiInfo_t amiInfo; */
/* } NfcConfSetV2_t; */

// For Terminal message format (protocol version 3)
typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 serialNum[SERIAL_NUM_LEN];
	uint8 sleepMode;
	uint8 year[2];
	uint8 mon;
	uint8 day;
	uint8 hour;
	uint8 min;
	uint8 sec;
	uint8 meterInterval;
	uint8 reportInterval;
	uint8 messageFrame;
#if NBIOT_DEVICE
	uint8 serviceCode[4];
	uint8 serverIp[4];
	uint8 serverPort[2];
#endif
	uint8 meterNum;
	NfcMeterInfo_t meterInfo[3];
} NfcConfSetV3_t;

// For Repeator message format (protocol version 4)
typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 serialNum[SERIAL_NUM_LEN];
	uint8 sleepMode;
	uint8 dateTime[4];
	uint8 meterInterval;
	uint8 reportInterval;
	uint8 messageFrame;
#if NBIOT_DEVICE
	uint8 serviceCode[4];
	uint8 serverIp[4];
	uint8 serverPort[2];
#endif
	NfcAmiInfo_t amiInfo;
} NfcConfSetV4_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 meterPort;
	uint8 year[2];
	uint8 mon;
	uint8 day;
	uint8 hour;
	uint8 min;
	uint8 sec;
} NfcMeterReq_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 serialNum[SERIAL_NUM_LEN];
	uint8 reqMode;
	uint8 fwVer[4];
} NfcFwUpdateReq_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 result;
	uint8 state;
	uint8 serialNum[SERIAL_NUM_LEN];
	uint8 fwVer[4];
} NfcFwUpdateResp_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 reqType;
} NfcServerConnReq_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 reset;
	uint8 sleepMode;
} NfcBdCtrlReqV1_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 reset;
	uint8 sleepMode;
	uint8 reportMode;
} NfcBdCtrlReqV2_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 reset;
	uint8 sleepMode;
	uint8 reportMode;
	uint8 periodMode;
} NfcBdCtrlReqV3_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 reset;
	uint8 sleepMode;
	uint8 reportMode;
	uint8 periodMode;
	uint8 debugMode;
} NfcBdCtrlReqV4_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 meterport;
	uint8 year[2];
	uint8 mon;
	uint8 day;
	uint8 hour;
	uint8 min;
	uint8 sec;
	uint8 from_year[2];
	uint8 from_mon;
	uint8 from_day;
	uint8 to_year[2];
	uint8 to_mon;
	uint8 to_day;
} NfcPeriodMeterReq_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 serialNum[SERIAL_NUM_LEN];
	uint8 meterType;
	uint8 meterPort;
	uint8 meterValue[4];
	uint8 year[2];
	uint8 mon;
	uint8 day;
	uint8 hour;
	uint8 min;
	uint8 sec;
} NfcPulseMeterValueSet_t;

#define IGNORE_SERIAL_NUM 0
#define CHANGE_SERIAL_NUM 1
#define CHECK_SERIAL_NUM 2

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 serialNum[SERIAL_NUM_LEN]; // 0 - no change
	uint8 whatWithSN;
	uint8 bodyLen;
	uint8 body[39]; // 39 = 64-10-1-1-12-1
} NfcMeterAdjustReq_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
} NfcSlaveCheckReq_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
} NfcNbiotIdReq_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 serviceCode[4];
} NfcNbiotIdSet_t;

typedef struct {
	NfcHeader_t header; // 10
	uint8 mtype; // 1
	uint8 mversion; // 1
} NfcFlashDateListReq_t; // 12

typedef struct {
	NfcHeader_t header; // 10
	uint8 mtype; // 1
	uint8 mversion; // 1
	uint8 yearFrom; // 1
	uint8 monFrom; // 1
	uint8 dayFrom; // 1
	uint8 yearTo; // 1
	uint8 monTo; // 1
	uint8 dayTo; // 1
} NfcFlashDataReq_t; // 18

//////////////////////
///// Send
//////////////////////

// For Terminal message format (protocol version 1)
typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 reportType;
	uint8 result;
	uint8 errorCode;
	uint8 serialNum[SERIAL_NUM_LEN];
	uint8 sleepMode;
	uint8 reserved;
#if LORA_DEVICE
	uint8 appEui[8];
#else // NBIOT_DEVICE
	uint8 imsi[8];
	uint8 serverIp[4];
	uint8 serverPort[2];
#endif
	uint8 batt;
	uint8 fwVer[4];
	uint8 meterInterval;
	uint8 reportInterval;
	uint8 year[2];
	uint8 mon;
	uint8 day;
	uint8 hour;
	uint8 min;
	uint8 sec;
	uint8 nMeter;
	NfcMeterInfo_t meterInfo[3];
} NfcConfReportV1_t;

// For Repeator message format (protocol version 2)
typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 reportType;
	uint8 result;
	uint8 errorCode;
	uint8 serialNum[SERIAL_NUM_LEN];
	uint8 sleepMode;
	uint8 reserved;
#if LORA_DEVICE
	uint8 appEui[8];
#else // NBIOT_DEVICE
	uint8 imsi[8];
	uint8 serverIp[4];
	uint8 serverPort[2];
#endif
	uint8 batt;
	uint8 fwVer[4];
	uint8 meterInterval;
	uint8 reportInterval;
	NfcAmiInfo_t amiInfo;
	uint8 dateTime[4];
} NfcConfReportV2_t;

typedef struct {
	uint8 type;
	uint8 port;
	uint8 flag;
	uint8 serial[4];
	uint8 value[4];
	uint8 status[2];
} NfcOneMeterReport_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 batt;
	uint8 year[2];
	uint8 mon;
	uint8 day;
	uint8 hour;
	uint8 min;
	uint8 sec;
	uint8 meterState;
	uint8 nMeter;
	NfcOneMeterReport_t oneMeter[3];
} NfcMeterReport_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 currentYear[2];
	uint8 currentMon;
	uint8 currentDay;
	uint8 currentHour;
	uint8 currentMin;
	uint8 currentSec;
	uint8 lastAccessYear[2];
	uint8 lastAccessMon;
	uint8 lastAccessDay;
	uint8 lastAccessHour;
	uint8 lastAccessMin;
	uint8 lastAccessSec;
	uint8 serverAccessState;
	uint8 accessStepOrResult;
	uint8 rssi;
	uint8 snr;
} NfcServerConnReportV1_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 currentYear[2];
	uint8 currentMon;
	uint8 currentDay;
	uint8 currentHour;
	uint8 currentMin;
	uint8 currentSec;
	uint8 lastAccessYear[2];
	uint8 lastAccessMon;
	uint8 lastAccessDay;
	uint8 lastAccessHour;
	uint8 lastAccessMin;
	uint8 lastAccessSec;
	uint8 serverAccessState;
	uint8 accessStepOrResult;
	uint8 rssi;
	uint8 snr;
	uint8 rsrp;
	uint8 rsrq;
} NfcServerConnReportV2_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 reset;
	uint8 sleepMode;
} NfcBdCtrlAckV1_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 reset;
	uint8 sleepMode;
	uint8 reportMode;
} NfcBdCtrlAckV2_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 reset;
	uint8 sleepMode;
	uint8 reportMode;
	uint8 periodMode;
} NfcBdCtrlAckV3_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 reset;
	uint8 sleepMode;
	uint8 reportMode;
	uint8 periodMode;
	uint8 debugMode;
} NfcBdCtrlAckV4_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 fwVer[4];
} NfcFwVerReport_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 fwVer[4];
	uint8 serialNum[SERIAL_NUM_LEN]; // 0 - no change
	uint8 deviceCode;
	uint8 deviceType;
} NfcDevInfoReport_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 totalBlock;
	uint8 currentBlock;
	uint8 meterNum;
	uint8 meterInfo[45];
} NfcPeriodMeterReport_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
} NfcPulseMeterValueSetAck_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 serialNum[SERIAL_NUM_LEN]; // 0 - no change
	uint8 fwVer[4];
	uint8 bodyLen;
	uint8 body[35]; // 35 = 64-10-1-1-12-4-1
} NfcMeterAdjustResp_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 serialNum[SERIAL_NUM_LEN]; // 0 - no change
	uint8 fwVer[4];
	uint8 result;
} NfcMeterAdjustError_t;

#if defined(AUX_REPEATER)
typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 checkState;
	uint8 slaveSerialNum[LEN_SLAVE_SERIAL_NUM];
	uint8 nwkAddr[LEN_NETWORK_ADDRESS];
	uint8 slaveBatt;
	uint8 slaveRssi;
	uint8 masterRssi;
} NfcSlaveCheckReport_t;
#endif

#if (defined(NBIOT_LG_TYPE) || defined(NBIOT_BASE_TYPE))
typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 reportType;
	uint8 serviceCode[4];
	uint8 cseid[LEN_MODEM_EP_NAME];
	uint8 iccid[10];
} NfcNbiotIdReport_t;
#endif

#if LORA_DEVICE
#define JOIN_MODE_SEUDO_JOIN 0
#define JOIN_MODE_REAL_JOIN 1

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 serialNum[SERIAL_NUM_LEN];
	uint8 joinMode;
	uint8 appEui[8];
	uint8 appKey[16];
} NfcLoRaAppEuiKeySet_t;

typedef struct {
	NfcHeader_t header;
	uint8 mtype;
	uint8 mversion;
	uint8 reportType;
	uint8 result;
	uint8 errorCode;
	uint8 serialNum[SERIAL_NUM_LEN];
	uint8 fwVer[4];
	uint8 appEui[8];
	uint8 appKey[16];
} NfcLoRaAppEuiKeyReport_t;
#endif

typedef struct {
	NfcHeader_t header; // 10
	uint8 mtype; // 1
	uint8 mversion; // 1
	uint8 resultState; // 1
	uint8 nMonth; // 1
	uint8 dayList[30]; // 30 ( 5 days * 6 bytes)
} NfcFlashDateListReport_t; // 44

typedef struct {
	NfcHeader_t header; // 10
	uint8 mtype; // 1
	uint8 mversion; // 1
	uint8 resultState; // 1
	uint8 nTotal; // 1
	uint8 nCurrent; // 1
	uint8 caliber_dp; // 1
	uint8 year; // 1
	uint8 mon; // 1
	uint8 day; // 1
	uint8 hour; // 1
	uint8 refPos; // 1
	uint8 meterData[4]; // 4
	uint8 diff[12][2]; // 24
} NfcFlashDataReport_t; // 49

// Functio Prototype

void NFCAPP_runMessage(byte *data, uint8 len);
void NFCAPP_meterAdjustResp(uint8 *body, int bodyLen);
void NFCAPP_meterAdjustReq();
void NFCAPP_clearAsyncCmd();
void NFCAPP_continueSend(byte *p, int len);

#endif // _MESSAGE_HEADER_

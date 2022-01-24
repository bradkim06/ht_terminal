#ifndef __METER_H__
#define __METER_H__

#include "lcdDriver.h"

//=======================================
// Water Meter Type
//=======================================
#define W_STANDARD_D 0x01
#define W_HITEC_D 0x10
#define W_SHINHAN_D 0x20
#define W_SHINHAN_D_BIG 0x22
#define W_MNS_D 0x30
#define W_PRIMO 0x40
#define W_BADGER 0x42
#define W_ONETL_D 0x50

#define W_PULSE_1000L 0x71
#define W_PULSE_500L 0x72
#define W_PULSE_100L 0x73
#define W_PULSE_50L 0x74
#define W_PULSE_10L 0x75
#define W_PULSE_5L 0x76
#define W_PULSE_1L 0x77
#define W_PULSE_05L 0x78

//=======================================
// Gas Meter Type
//=======================================
#define G_STANDARD_D 0x02
#define G_ONETL_D 0x51

#define G_PULSE_1000L 0xA1
#define G_PULSE_500L 0xA2
#define G_PULSE_100L 0xA3
#define G_PULSE_50L 0xA4
#define G_PULSE_10L 0xA5
#define G_PULSE_5L 0xA6
#define G_PULSE_1L 0xA7
#define G_PULSE_05L 0xA8

//=======================================
// Calori Meter Type - 열량
//=======================================
#define C_STANDARD_D 0x03
#define C_ONETL_D 0x52

#define C_PULSE_1000L 0x91
#define C_PULSE_500L 0x92
#define C_PULSE_100L 0x93
#define C_PULSE_50L 0x94
#define C_PULSE_10L 0x95
#define C_PULSE_5L 0x96
#define C_PULSE_1L 0x97
#define C_PULSE_05L 0x98

//=======================================
// Hot Water Meter Type
//=======================================
#define H_STANDARD_D 0x04
#define H_ONETL_D 0x53

#define H_PULSE_1000L 0x81
#define H_PULSE_500L 0x82
#define H_PULSE_100L 0x83
#define H_PULSE_50L 0x84
#define H_PULSE_10L 0x85
#define H_PULSE_5L 0x86
#define H_PULSE_1L 0x87
#define H_PULSE_05L 0x88

//=======================================
// Electric Meter Type
//=======================================
#define E_PULSE_1000W 0x61
#define E_PULSE_100W 0x62
#define E_PULSE_10W 0x63
#define E_PULSE_8W 0x64
#define E_PULSE_4W 0x65
#define E_PULSE_2W 0x66
#define E_PULSE_1W 0x67
#define E_PULSE_04W 0x68
#define E_PULSE_02W 0x69
#define E_PULSE_01W 0x6A

#define METER_TYPE_W_PULSE(x)                                                                      \
	(x == W_PULSE_05L || x == W_PULSE_1L || x == W_PULSE_5L || x == W_PULSE_10L ||             \
	 x == W_PULSE_50L || x == W_PULSE_100L || x == W_PULSE_500L || x == W_PULSE_1000L)

#define METER_TYPE_E_PULSE(x)                                                                      \
	(x == E_PULSE_01W || x == E_PULSE_02W || x == E_PULSE_04W || x == E_PULSE_1W ||            \
	 x == E_PULSE_2W || x == E_PULSE_4W || x == E_PULSE_8W || x == E_PULSE_10W ||              \
	 x == E_PULSE_100W || x == E_PULSE_1000W)

#define METER_TYPE_H_PULSE(x)                                                                      \
	(x == H_PULSE_05L || x == H_PULSE_1L || x == H_PULSE_5L || x == H_PULSE_10L ||             \
	 x == H_PULSE_50L || x == H_PULSE_100L || x == H_PULSE_500L || x == H_PULSE_1000L)

#define METER_TYPE_C_PULSE(x)                                                                      \
	(x == C_PULSE_05L || x == C_PULSE_1L || x == C_PULSE_5L || x == C_PULSE_10L ||             \
	 x == C_PULSE_50L || x == C_PULSE_100L || x == C_PULSE_500L || x == C_PULSE_1000L)

#define METER_TYPE_G_PULSE(x)                                                                      \
	(x == G_PULSE_05L || x == G_PULSE_1L || x == G_PULSE_5L || x == G_PULSE_10L ||             \
	 x == G_PULSE_50L || x == G_PULSE_100L || x == G_PULSE_500L || x == G_PULSE_1000L)

#define METER_TYPE_PULSE(x)                                                                        \
	(METER_TYPE_W_PULSE(x) || METER_TYPE_E_PULSE(x) || METER_TYPE_H_PULSE(x) ||                \
	 METER_TYPE_C_PULSE(x) || METER_TYPE_G_PULSE(x))

#define METER_TYPE_STANDARD(x)                                                                     \
	(x == W_STANDARD_D || x == G_STANDARD_D || x == C_STANDARD_D || x == H_STANDARD_D)

#define METER_TYPE_ONETL(x) (x == W_ONETL_D || x == G_ONETL_D || x == C_ONETL_D || x == H_ONETL_D)

#define METER_TYPE_UART(x)                                                                         \
	(x == W_HITEC_D || x == W_SHINHAN_D || x == W_SHINHAN_D_BIG || x == W_MNS_D ||             \
	 METER_TYPE_STANDARD(x) || x == W_ONETL_D || x == G_ONETL_D || x == C_ONETL_D ||           \
	 x == H_ONETL_D)

#define METER_TYPE_CHECK(x)                                                                        \
	(METER_TYPE_UART(x) || METER_TYPE_PULSE(x) || x == W_PRIMO || x == W_BADGER)

//=======================================
// METER Point Num
//=======================================
#define METER_POINT_0(x)                                                                           \
	(x == W_PULSE_500L || x == W_PULSE_1000L || x == H_PULSE_500L || x == H_PULSE_1000L ||     \
	 x == C_PULSE_500L || x == C_PULSE_1000L || x == G_PULSE_500L || x == G_PULSE_1000L)

#define METER_POINT_1(x)                                                                           \
	(x == W_PULSE_50L || x == W_PULSE_100L || x == H_PULSE_50L || x == H_PULSE_100L ||         \
	 x == C_PULSE_50L || x == C_PULSE_100L || x == G_PULSE_50L || x == G_PULSE_100L)

#define METER_POINT_2(x)                                                                           \
	(x == W_PULSE_5L || x == W_PULSE_10L || x == H_PULSE_5L || x == H_PULSE_10L ||             \
	 x == C_PULSE_5L || x == C_PULSE_10L || x == G_PULSE_5L || x == G_PULSE_10L ||             \
	 METER_TYPE_E_PULSE(x) || x == W_SHINHAN_D_BIG)

#define METER_POINT_3(x)                                                                           \
	(x == W_PULSE_05L || x == W_PULSE_1L || x == H_PULSE_05L || x == H_PULSE_1L ||             \
	 x == C_PULSE_05L || x == C_PULSE_1L || x == G_PULSE_05L || x == G_PULSE_1L ||             \
	 x == W_ONETL_D || x == G_ONETL_D || x == C_ONETL_D || x == H_ONETL_D)

//=======================================
// METER Type 05L
//=======================================
#define METER_TYPE_05L_PULSE(x)                                                                    \
	(x == W_PULSE_05L || x == W_PULSE_5L || x == W_PULSE_50L || x == W_PULSE_500L ||           \
	 x == H_PULSE_05L || x == H_PULSE_5L || x == H_PULSE_50L || x == H_PULSE_500L ||           \
	 x == C_PULSE_05L || x == C_PULSE_5L || x == C_PULSE_50L || x == C_PULSE_500L ||           \
	 x == G_PULSE_05L || x == G_PULSE_5L || x == G_PULSE_50L || x == G_PULSE_500L)

//=======================================
// METER Port
//=======================================
#define METER_PORT_UART_1 0x01
#define METER_PORT_UART_2 0x02
#define METER_PORT_PULSE_1 0x11
#define METER_PORT_PULSE_2 0x12
#define METER_PORT_RS232_1 0x21
#define METER_PORT_RS485_1 0x31
#define METER_PORT_DPLC_1 0x41
//=======================================

//=======================================
// METER 05L 단위
//=======================================

enum { METERING_TYPE_NONE = 0,
       PDA_DATA_REQ_TYPE_METER_DB, // must be 1
       PDA_DATA_REQ_TYPE_STATUS,
       PDA_DATA_REQ_TYPE_METER_ONDEMAND,
       PDA_DATA_REQ_TYPE_AMR,
       PDA_DATA_REQ_TYPE_NUSU_DETECT,
       PDA_DATA_REQ_TYPE_GROUP_METER,
       PDA_DATA_REQ_TYPE_METER_SERIAL,
       METERING_TYPE_CAR,
       METERING_TYPE_SAVE,
       METERING_TYPE_PERIODIC,
       PDA_MASTER_METER_REQ,
};

#define METERING_REQ_FROM_PDA(x)                                                                   \
	(x == PDA_DATA_REQ_TYPE_METER_DB || x == PDA_DATA_REQ_TYPE_STATUS ||                       \
	 x == PDA_DATA_REQ_TYPE_METER_ONDEMAND || x == PDA_DATA_REQ_TYPE_AMR ||                    \
	 x == PDA_DATA_REQ_TYPE_METER_SERIAL || x == METERING_TYPE_CAR)

//**************************************
// Standard  Meter 파라미터
//**************************************
typedef struct {
	uint8 DIF;
	uint8 VIF;
} StdMeterParam_t;

//**************************************
// Onetl  Meter 파라미터
//**************************************
typedef struct {
	uint8 flowValue[4];
	uint8 st[2];
	uint8 rt[2];
} OntelMeterParam_t;

//**************************************
// 검침 데이터 저장
//**************************************

typedef struct {
	uint8 year;
	uint8 mon;
	uint8 day;
	uint8 hour;
	uint8 min;
	uint8 sec;
	uint8 isTimeSync;
	uint8 meterData[4];
	uint8 meterStatus;
	uint8 st[2];
	uint8 rt[2];
	uint8 flowData[4];
	LcdIcon_t icon;
} MeterUnitData_t;

typedef struct {
	int nData;
	int saveInterval;
	uint8 caliberDp;
	uint8 dif;
	uint8 vif;
	uint8 meterSerial[4];
	MeterUnitData_t unit[MAX_NUM_STORED_DATA];
	MeterUnitData_t lastDiffUnit;
} MeterStoredData_t;

typedef struct {
	uint8 caliberDp;
	uint8 dif;
	uint8 vif;
	uint8 meterSerial[4];
	MeterUnitData_t unit;
#if defined(AUX_REPEATER)
	uint8 noResponseFromSlave;
#endif
} MeterTempData_t;

//**************************************
// 검침 동작을 위한 파라미터
//**************************************

// 공통 파라미터
typedef struct {
	uint8 nRetry;
	uint8 forMeterAdjust;
	uint8 inProcess;
	uint8 meteringType;
	uint8 meterType;
	uint8 success;
#if defined(AUX_REPEATER)
	uint8 slaveMeteringError;
#endif
} Metering_t;

enum { NOT_ACIVE_METERING = 0,
       INITIAL_METERING,
       PERIODIC_METERING,
       PERIODIC_METERING_FOR_SAVE,
       IMMEDIATE_METERING,
       IMMEDIATE_METERING_FOR_REPORT,
};

//**************************************
// 검침 데이터 핸들링을 위한 구조체
//**************************************

typedef struct {
	uint8 startByte1; // fixed value - 0x2a('*')
	uint8 userID[10];
	uint8 meterValue[8];
	uint8 status[2];
	uint8 carriageReturn1;
	uint8 startByte2; // fixed value - 0x26('&')
	uint8 version[2];
	uint8 etc[30];
	uint8 checksum[2];
	uint8 carriageReturn2;
} MnsMeter_t;

#define MNS_CHECKSUM_START1 2
#define MNS_CHECKSUM_STOP1 11
#define MNS_CHECKSUM_START2 12
#define MNS_CHECKSUM_STOP2 21
#define MNS_CHECKSUM_START3 24
#define MNS_CHECKSUM_STOP3 55

typedef struct {
	uint8 year; // 2000년 이후
	uint8 mon;
	uint8 day;
	uint8 hour;
	uint8 min;
} MeterDate_t;

typedef struct {
	MeterDate_t meterDate;
	uint8 meterSerial[10];
	uint8 meterValue[4];
	uint8 batt;
	uint8 status;
} MeterDb_t;

/*********************************************
 *  saved meter data in RAM and flash memory
 *********************************************
 */

#define MAX_NUM_SAVED_METER_DATA 24
#define MAX_NUM_SAVED_PRIMO_DATA 12
#define MAX_ERR_DAY 2
#define MAX_METER_NUM 3

typedef struct {
	uint8 meterValue[4];
	uint8 batt;
	uint8 status;
	uint8 serial[4];
} MeterStdData_t;

typedef struct {
	uint8 meterValue[4];
	uint8 batt;
	uint8 status;
	uint8 serial[4];
} MeterHitecData_t;

typedef struct {
	uint8 meterValue[4];
	uint8 batt;
	uint8 status;
	uint8 serial[4];
} MeterShData_t;

typedef struct {
	uint8 meterValue[4];
	uint8 batt;
	uint8 status;
	uint8 serial[4];
} MeterOntelData_t;

typedef struct {
	uint8 meterValue[4];
	uint8 reserved[6];
} MeterPulsData_t;

typedef struct {
	uint8 meterValue[6];
	uint8 backflow[4];
} MeterPrimoData_t;

typedef struct {
	uint8 year; // 2000년 이후
	uint8 mon;
	uint8 day;
	uint8 hour;
	uint8 data[10];
	BOOL mtdown;
	uint8 slaveBatt; // padding bytes // primo meter 시 batt 저장 용도로 사용
} MeterData_t;

#define SHINHAN_METER_FRAME_LEN 15
#define SHINHAN_BIG_METER_OFFSET 2

#define SHINHAN_METER_FRAME_LEN 15
#define SHINHAN_BIG_METER_FRAME_LEN (SHINHAN_METER_FRAME_LEN + SHINHAN_BIG_METER_OFFSET)

#define SHINHAN_METER_DATA_LEN 11
#define SHINHAN_BIG_METER_DATA_LEN (SHINHAN_METER_DATA_LEN + SHINHAN_BIG_METER_OFFSET)

#define SHINHAN_METER_SERIAL_LEN 4
#define SHINHAN_BIG_METER_SERIAL_LEN 6

typedef struct {
	uint8 irq_ack; // always 0x06
	uint8 stx; // always 0x02
	uint8 len; // always 0x0d
	uint8 serial[6];
	uint8 value[4];
	uint8 battery;
	uint8 status;
	uint8 etx; // always 0x03
	uint8 checksum;
} shBig_t;

typedef struct {
	uint8 irq_ack; // always 0x06
	uint8 stx; // always 0x02
	uint8 len; // always 0x0b
	uint8 serial[4];
	uint8 value[4];
	uint8 battery;
	uint8 status;
	uint8 etx; // always 0x03
	uint8 checksum;
} sh_t;

// meter type
#define MT_15mm 0x01
#define MT_20mm 0x02
#define MT_25mm 0x03
#define MT_32mm 0x04
#define MT_40mm 0x05
#define MT_50mm 0x06
#define MT_80mm 0x07
#define MT_100mm 0x08
#define MT_150mm 0x09
#define MT_200mm 0x0A
#define MT_250mm 0x0B
#define MT_300mm 0x0C

//=======================================
// STANDARD METER MSG 정의
#define STD_METER_DATA_REQ 0x5B // terminal --> meter
#define STD_METER_DATA_RESP 0x08 // meter --> terminal
#define STD_METER_CONFIG_REQ 0xA2 // terminal --> meter
#define STD_METER_ADJUST_REQ 0xA0 // terminal --> meter
#define STD_METER_ADJUST_RESP 0xB2 // meter --> terminal
#define STD_LCD_TEST_REQ 0xA6 // terminal --> meter
#define STD_LCD_MARK_REQ 0xA7 // terminal --> meter

// adjust frame result code
#define STD_METER_ADJUST_SUCCESS 0 // Others value is fail.

// frame delimiter (terminal --> meter)
#define STD_REQ_START 0x10
#define STD_REQ_ADDR 0x01
#define STD_REQ_CHECKSUM (STD_METER_DATA_REQ + STD_REQ_ADDR)
#define STD_REQ_STOP 0x16

// frame delimiter (meter --> terminal)
#define STD_RESP_START 0x68
#define STD_RESP_STOP 0x16

typedef struct {
	uint8 stx;
	uint8 cfield; // STD_METER_DATA_REQ
	uint8 afield;
	uint8 checksum;
	uint8 etx;
} StdMeterReq_t;

typedef struct {
	uint8 stx;
	uint8 cfield; // STD_METER_DISPLAY_LCD
	uint8 afield;
	uint8 pattern; // 1 - 전체 표시
	uint8 checksum;
	uint8 etx;
} StdMeterLcd_t;

typedef struct {
	uint8 stx1;
	uint8 l1field; // always 0xff
	uint8 l2field; // always 0xff
	uint8 stx2;
	uint8 cfield; // STD_METER_DATA_RESP
	uint8 afield;
	uint8 cifield;
	uint8 mdh;
	uint8 serial[4];
	uint8 status;
	uint8 dif;
	uint8 vif;
	uint8 value[4];
	uint8 checksum;
	uint8 etx;
} StdMeterDataResp_t;

typedef struct {
	uint8 stx;
	uint8 cfield; // STD_METER_ADJUST_REQ
	uint8 afield;
	uint8 caliber;
	uint8 serial[4];
	uint8 q3[2];
	uint8 qt[2];
	uint8 q2[2];
	uint8 q1[2];
	uint8 maker;
	uint8 checksum;
	uint8 etx;
} StdMeterAdjustReq_t;

typedef struct {
	uint8 stx1;
	uint8 l1field; // always 0x0f
	uint8 l2field; // always 0x0f
	uint8 stx2;
	uint8 cfield; // STD_METER_ADJUST_RESP
	uint8 caliber;
	uint8 serial[4];
	uint8 q3[2];
	uint8 qt[2];
	uint8 q2[2];
	uint8 q1[2];
	uint8 maker;
	uint8 fwVersion;
	uint8 errorCode;
	uint8 checksum;
	uint8 etx;
} StdMeterAdjustResp_t;

// configuration result structure parameter
typedef struct {
	uint8 caliber;
	uint8 serial[4];
	uint8 q3[2];
	uint8 qt[2];
	uint8 q2[2];
	uint8 q1[2];
	uint8 maker;
	uint8 fwVersion;
	uint8 errorCode;
} StdAdjustResult_t;

typedef struct {
	uint8 stx;
	uint8 cfield; // STD_LCD_TEST_REQ
	uint8 afield;
	uint8 data;
	uint8 time;
	uint8 checksum;
	uint8 etx;
} StdLcdTestReq_t;

typedef struct {
	uint8 stx;
	uint8 cfield; // STD_LCD_MARK_REQ
	uint8 afield;
	uint8 data;
	uint8 checksum;
	uint8 etx;
} StdLcdMarkReq_t;

//=======================================
// ONETL METER MSG 정의

// Meter message ID
#define ONETL_ID_READ_METER 0xA5 // Only for Calorie Meter
#define ONETL_ID_READ_ST_RT 0x5F // Only for Calorie Meter
#define ONETL_ID_READ_FLOW 0x5D // Common request message
#define ONETL_ID_READ_TOTAL 0xAA // Only for Calorie Meter

// Meter address
#define ONETL_W_METER_ADDR 0x02 // Water Meter address
#define ONETL_H_METER_ADDR 0x03 // Hot water Meter address
#define ONETL_G_METER_ADDR 0x04 // GAS Meter address
#define ONETL_C_METER_ADDR 0x05 // Calorie Meter address

// frame delimiter (terminal --> meter)
#define ONETL_REQ_STX 0xE1
#define ONETL_REQ_ETX 0xF0

// frame delimiter (meter --> terminal)
#define ONETL_RESP_STX 0xE9
#define ONETL_RESP_ETX 0xF0

// Battery status
#define ONETL_STATUS_BATT_EMPTY 0x00
#define ONETL_STATUS_BATT_LOW 0x01
#define ONETL_STATUS_BATT_FULL 0x02

// BCC AND operatin constant
#define ONETL_BCC_AND_CONSTANT 0x7F

typedef struct {
	uint8 stx;
	uint8 id;
	uint8 len;
	uint8 address;
} OntelHeader_t;

typedef struct {
	uint8 stx;
	uint8 id; // STD_METER_DATA_REQ
	uint8 len;
	uint8 address;
	uint8 bcc;
	uint8 etx;
} OntelReq_t;

typedef struct {
	uint8 stx;
	uint8 id;
	uint8 len;
	uint8 address;
	uint8 serial[4];
	uint8 meterValue[5];
	uint8 status;
	uint8 bcc;
	uint8 etx;
} OntelFlowResp_t;

typedef struct {
	uint8 stx;
	uint8 id;
	uint8 len;
	uint8 address;
	uint8 serial[4];
	uint8 meterValue[5];
	uint8 st[2];
	uint8 rt[2];
	uint8 flowValue[5];
	uint8 status;
	uint8 bcc;
	uint8 etx;
} OntelTotalResp_t;

//=======================================

char *METER_getMeterName(int meterType);

BOOL METER_sendRequest(uint8 meterType);
BOOL METER_recvResponse(uint8 meterType, MeterUnitData_t *pUnit);
BOOL METER_std_configRequest();
BOOL METER_std_adjustReq(char *sn, uint8 caliber, uint8 maker, uint16 q3, uint16 qt, uint16 q2,
			 uint16 q1);
BOOL METER_std_adjustResp(StdAdjustResult_t *result);
BOOL METER_std_lcdTestReq();
BOOL METER_std_lcdMarkReq(BOOL isMarkOn);

void METER_deleteAllData();
void METER_clearStoredData();
int METER_getNumberOfStoredData();
int METER_fillSendData(uint8 *pData);
void METER_saveInFlash(Date_t *pDate, MeterUnitData_t *pUnit);
void METER_displayStoredData();
void METER_addStoredData(Date_t *pDate, MeterUnitData_t *pUnit);
void METER_collectStoredData();
void METER_displayTempData();
void METER_addTempData(Date_t *pDate, MeterUnitData_t *pUnit);
void METER_copySerialNum(uint8 *pSerial);
void METER_enable(uint8 meterType);
void METER_disable();
uint8 *METER_getSerialNum();
void METER_getMeterData(int index, uint8 *buf);
MeterUnitData_t *METER_getTimeStampOfData(int index);
uint8 METER_getMeterCaliber_dp();
uint8 METER_getMeterStatus(int index);
MeterUnitData_t *METER_getStoredData(int index);
MeterUnitData_t *METER_getTempData();
MeterUnitData_t *METER_getLastData();
int METER_getFirstValidPos();
BOOL METER_isAllFF(uchar *p, int len);
int METER_getSaveInterval();
int METER_needNewMetering(int secLimit);

void METER_bypassReq(uint8 *body, int bodyLen);
BOOL METER_bypassResp();
void METER_saveMeterInfo(uint8 *serial, uint8 caliberDp, uint8 dif, uint8 vif);

#endif //__METER_H__

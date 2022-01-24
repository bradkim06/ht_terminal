#ifndef __MESSAGE_H__
#define __MESSAGE_H__

/*************************************************************************************
 Modem 제어 시퀀스에는 다음의 3가지 경우가 있음

 (1) 부팅 후 Join
 (2) 주기적 보고
 (3) Reed sensor 또는 NFC 제어에 의한 Join
*********************************************************************/

typedef struct {
	uchar rssi;
	uchar berSNR;
} LoRaRadioQuality_t;

typedef struct {
	uchar termSerial[5];
	uchar fwVer[2];
	uchar termBatt;
} LoRaDevInfo_t;

typedef struct {
	uchar meterSerial[4];
	uchar meterType;
	uchar meterCaliber_dp;
	uchar meterStatus;
} LoRaMeterInfo_t;

typedef struct {
	uchar meterType;
	uchar meterCaliber_dp;
	uchar meterStatus;
	uchar termBatt;
} LoRaMeterDevInfo_t;

typedef struct {
	uchar mi;
	uchar ri;
} LoRaMeteringParam_t;

typedef struct {
	uchar year;
	uchar mon;
	uchar day;
	uchar hour;
	uchar min;
	uchar sec;
} LoRaTimeStamp_t;

typedef struct {
	uchar interval;
	uchar numData;
	uchar refValuePos;
	uchar refValue[4];
	uchar valueDiff[NUM_LORA_STORED_DATA][2];
} LoRaMeterData_t;

typedef struct {
	uchar resetCause;
	uchar resetCount[2];
} LoRaReset_t;

//------------------------
// Message for LoRa_HITEC
//------------------------
#define LORA_JOIN 0x10
#define LORA_DATA_REPORT 0x30
#define LORA_AS923_DATA_REPORT 0x31

#define PROTOCOL_VERSION 0xA1

typedef struct {
	uchar protocol; // 1
	uchar len; // 1 -  2
	uchar mtype; // 1 -  3
	LoRaRadioQuality_t radio; // 2 -  5
	LoRaDevInfo_t term; // 8 - 13
	LoRaMeterInfo_t meter; // 7 - 20
	LoRaMeteringParam_t mp; // 2 - 22
	LoRaTimeStamp_t meterTime; // 6 - 28
	uchar data[4]; // 4 - 32
	LoRaReset_t reset; // 3 - 35
	uchar checksum; // 1 - 36
} LoraJoin_t;

// data 갯수 n은 1~6
typedef struct {
	uchar protocol; // 1
	uchar len; // 1  - 2
	uchar mtype; // 1  - 3
	LoRaRadioQuality_t radio; // 2  - 5
	LoRaDevInfo_t term; // 8  - 13
	LoRaMeterInfo_t meter; // 7  - 20
	LoRaMeteringParam_t mp; // 2  - 22
	LoRaTimeStamp_t meterTime; // 6  - 28
	LoRaMeterData_t data; // 7 + nData*2 = 9~19 - 37~46
	uchar checksum; // 1 -  38 ~ 47
} LoraDataReport_t;

// data 갯수 n은 1~12
typedef struct {
	uchar protocol; // 1
	uchar len; // 1  - 2
	uchar mtype; // 1  - 3
	LoRaRadioQuality_t radio; // 2  - 5
	LoRaMeterDevInfo_t info; // 4  - 9
	LoRaMeteringParam_t mp; // 2  - 11
	LoRaTimeStamp_t meterTime; // 6  - 17
	LoRaMeterData_t data; // 7 + nData*2 = 9~31 - 26~48
	uchar checksum; // 1 -  27 ~ 48
} LoraAs923DataReport_t;

#endif

#ifndef __MESSAGE_H__
#define __MESSAGE_H__

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
// Message for LoRa_SEOUL
//------------------------
#define LORA_JOIN 0x10
#define LORA_DATA_REPORT 0x30
#define LORA_DEVICE_CONTROL 0x50

#define PROTOCOL_VERSION 0xA1

#define LEN_MAX_LORA_DATA 62

typedef struct {
	uchar protocol;
	uchar len;
	uchar mtype;
	LoRaRadioQuality_t radio;
	LoRaDevInfo_t term;
	LoRaMeterInfo_t meter;
	LoRaMeteringParam_t mp;
	LoRaTimeStamp_t meterTime;
	uchar data[4];
	LoRaReset_t reset;
	uchar checksum;
} LoraJoin_t;

typedef struct {
	uchar protocol;
	uchar len; // 1
	uchar mtype; // 1
	LoRaRadioQuality_t radio; // 2
	LoRaDevInfo_t term; // 8
	LoRaMeterInfo_t meter; // 7
	LoRaMeteringParam_t mp; // 2
	LoRaTimeStamp_t meterTime; // 6
	LoRaMeterData_t data; // 7 + nData*2
	uchar checksum;
} LoraDataReport_t;

typedef struct {
	uchar protocol;
	uchar len;
	uchar mtype;
	uchar mi;
	uchar ri;
	uchar checksum;
} LoraDeviceControl_t;

int MODEM_sendDataReport(char *msg);
int MODEM_sendJoin(char *msg);
void MODEM_rcvDeviceControl(uchar *rxData, int rxLen);

#endif

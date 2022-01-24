#include <msp430.h>
#include <ctype.h>
#include <time.h>

#include "common_header.h"
#include "port_desc.h"
#include "uart.h"
#include "check_meter_misc.h"

#include "osal_Timer.h"
#include "app.h"
#include "meter.h"
#include "lcdDriver.h"
#include "shell.h"
#include "rtcAlarm.h"
#include "modem.h"
#include "message.h"
#include "battery.h"
#include "test.h"

extern Config_t conf;
extern uint8 AppProcess;

extern Modem_t modem;
extern ModemFlag_t mFlag;

static uchar cal_checksum(uchar *p, int len)
{
	uchar checksum = 0;

	for (int i = 0; i < len; i++) {
		checksum += *p++;
	}

	return checksum;
}

static int insert_radioQuality(LoRaRadioQuality_t *p)
{
	p->rssi = modem.lastRSSI;
	p->berSNR = modem.lastSNR;

	return sizeof(LoRaRadioQuality_t);
}

static int insert_termInfo(LoRaDevInfo_t *p)
{
	for (int i = 0; i < (SERIAL_NUM_LEN - 2) / 2; i++) { // 맨 앞 두자리는 제외
		p->termSerial[i] =
			ascii2BCD(conf.serialNum[2 + 2 * i + 0], conf.serialNum[2 + 2 * i + 1]);
	}

	// f/w version
	char fw_ver[0x10] = "";
	snprintf(fw_ver, 0x10, "%s", FIRMWARE_VER);
	fw_ver[0] = '0';

	p->fwVer[0] = ascii2BCD(fw_ver[0], fw_ver[1]);
	p->fwVer[1] = ascii2BCD(fw_ver[2], fw_ver[3]);

	p->termBatt = BATT_getVoltage();

	return sizeof(LoRaDevInfo_t);
}

static int insert_meterInfo(LoRaMeterInfo_t *p)
{
	uint8 *pSerial = METER_getSerialNum();
	if (pSerial) {
		memcpy(p->meterSerial, pSerial, 4);
	}

	p->meterType = conf.meterType;
	p->meterCaliber_dp = METER_getMeterCaliber_dp();

	MeterUnitData_t *pUnitData;

	if (AppProcess == APP_INITIAL_REPORT || AppProcess == APP_IMMEDIATE_REPORT) {
		pUnitData = METER_getTempData();
	} else {
		pUnitData = METER_getLastData();
	}
	p->meterStatus = pUnitData->meterStatus;

	return sizeof(LoRaMeterInfo_t);
}

static int insert_meter_device_info(LoRaMeterDevInfo_t *p)
{
	p->meterType = conf.meterType;
	p->meterCaliber_dp = METER_getMeterCaliber_dp();

	MeterUnitData_t *pUnitData;

	if (AppProcess == APP_INITIAL_REPORT || AppProcess == APP_IMMEDIATE_REPORT) {
		pUnitData = METER_getTempData();
	} else {
		pUnitData = METER_getLastData();
	}
	p->meterStatus = pUnitData->meterStatus;
	p->termBatt = BATT_getVoltage();

	return sizeof(LoRaMeterDevInfo_t);
}

static int insert_meteringParam(LoRaMeteringParam_t *p)
{
	p->mi = conf.meterInterval;
	p->ri = conf.reportInterval;

	return sizeof(LoRaMeteringParam_t);
}

static int insert_relativeTime(LoRaTimeStamp_t *p)
{
	MeterUnitData_t *pUnitData;
	if (AppProcess == APP_INITIAL_REPORT || AppProcess == APP_IMMEDIATE_REPORT) {
		pUnitData = METER_getTempData();
	} else {
		pUnitData = METER_getLastData();
	}

	Date_t now;
	RTC_read(&now);

	if (RTC_isTimeSync()) { // 나의 RTC가 유효 - 네트워크 시간에 동기
		p->year = now.year - 2000;
		p->mon = now.mon;
		p->day = now.day;
		p->hour = now.hour;
		p->min = now.min;
		p->sec = now.sec;
	} else {
		Date_t lastDTM;
		lastDTM.year = pUnitData->year + 2000;
		lastDTM.mon = pUnitData->mon;
		lastDTM.day = pUnitData->day;
		lastDTM.hour = pUnitData->hour;
		lastDTM.min = pUnitData->min;
		lastDTM.sec = 0;

		int minDiff = RTC_calcMinDiff(&lastDTM, &now);
		if (minDiff < 0 || minDiff > (250 * 60)) {
			minDiff = 0;
		}

		p->year = 0xFF;
		p->mon = 0xFF;
		p->day = 0xFF;
		p->hour = minDiff / 60;
		p->min = minDiff % 60;
		p->sec = 0x00;
	}

	return sizeof(LoRaTimeStamp_t);
}

static int insert_oneData(uchar *p)
{
	MeterUnitData_t *pUnitData = METER_getTempData();

	if (METER_isAllFF(pUnitData->meterData, 4)) { // meter down
		memcpy(p, pUnitData->meterData, 4);
	} else {
		uint32 value = 0;
		bcd2int(pUnitData->meterData, &value, 4);
		memcpy(p, &value, 4);
	}

	return 4;
}

static int insert_meterData(LoRaMeterData_t *p)
{
	METER_collectStoredData();

	int nData = METER_getNumberOfStoredData();
	if (nData > NUM_LORA_STORED_DATA) {
		nData = NUM_LORA_STORED_DATA;
	}

	p->interval = METER_getSaveInterval();
	p->numData = nData;

	memset(p->refValue, 0xFF, 4);
	for (int i = 0; i < nData; i++) {
		memset(p->valueDiff[i], 0xFF, 2);
	}

	int validPos = METER_getFirstValidPos();
	p->refValuePos = validPos;

	if (validPos != 0xFF) {
		uint8 valueArray[4];

		// 유효한 최신 데이터 채우기
		METER_getMeterData(validPos, valueArray);

		uint32 prevValue = 0;
		bcd2int(valueArray, &prevValue, 4);

		memcpy(p->refValue, &prevValue, 4);
		memset(p->valueDiff[validPos], 0, 2);

		for (int i = validPos + 1; i < nData; i++) {
			METER_getMeterData(i, valueArray);

			if (METER_isAllFF(valueArray, 4)) {
				// meter down
				memset(p->valueDiff[i], 0xFF, 2);
			} else {
				uint32 currValue = 0;
				bcd2int(valueArray, &currValue, 4);

				if (prevValue < currValue) { // 검침값이 줄어든 경우
					memset(p->valueDiff[i], 0, 2);
				} else {
					uint16 diff = prevValue - currValue;
					memcpy(p->valueDiff[i], &diff, 2);
					prevValue = currValue;
				}
			}
		}
	}

	return (7 + nData * 2);
}

static int insert_reset(LoRaReset_t *p)
{
	p->resetCause = 0;
	memcpy(p->resetCount, &conf.resetCount, 2);

	return sizeof(LoRaReset_t);
}

void MODEM_sendJoin()
{
	uchar msg[100];
	LoraJoin_t *p = (LoraJoin_t *)&msg;

	p->protocol = PROTOCOL_VERSION;

	int len = 0;
	p->mtype = LORA_JOIN;
	len++;

	len += insert_radioQuality(&p->radio);
	len += insert_termInfo(&p->term);
	len += insert_meterInfo(&p->meter);
	len += insert_meteringParam(&p->mp);
	len += insert_relativeTime(&p->meterTime);
	len += insert_oneData(p->data);
	len += insert_reset(&p->reset);

	p->len = len;
	p->checksum = cal_checksum(&p->mtype, len);

	MODEM_sendData(msg, sizeof(LoraJoin_t));
}

void MODEM_sendLoRaData()
{
	uchar msg[100];
	LoraDataReport_t *p = (LoraDataReport_t *)&msg;

	p->protocol = PROTOCOL_VERSION;

	int len = 0;
	p->mtype = LORA_DATA_REPORT;
	len++;

	len += insert_radioQuality(&p->radio);
	len += insert_termInfo(&p->term);
	len += insert_meterInfo(&p->meter);
	len += insert_meteringParam(&p->mp);
	len += insert_relativeTime(&p->meterTime);
	len += insert_meterData(&p->data);

	p->len = len;
	msg[len + 2] = cal_checksum(&p->mtype, len);

	MODEM_sendData(msg, len + 3); // 3- protocol, len, checksum
}

void MODEM_sendAS923Data()
{
	uchar msg[100];
	LoraAs923DataReport_t *p = (LoraAs923DataReport_t *)&msg;

	p->protocol = PROTOCOL_VERSION;

	int len = 0;
	p->mtype = LORA_AS923_DATA_REPORT;
	len++;

	len += insert_radioQuality(&p->radio);
	len += insert_meter_device_info(&p->info);
	len += insert_meteringParam(&p->mp);
	len += insert_relativeTime(&p->meterTime);
	len += insert_meterData(&p->data);

	p->len = len;
	msg[len + 2] = cal_checksum(&p->mtype, len);

	MODEM_sendData(msg, len + 3); // 3- mtype, len, checksum
}

void MODEM_sendDataReport()
{
	if (METER_getNumberOfStoredData() <= 6) {
		// 데이터 갯수가 적으면 단말/미터 정보를 상세히 전송
		MODEM_sendLoRaData();
	} else {
		// 데이터 갯수가 많으면 단말/미터 정보를 간단히 전송
		MODEM_sendAS923Data();
	}
}

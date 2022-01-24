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
	p->rssi = modem.sigQuality.rssi;
	p->berSNR = modem.sigQuality.snr;

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

static int insert_meteringParam(LoRaMeteringParam_t *p)
{
	p->mi = conf.meterInterval;
	p->ri = conf.reportInterval;

	return sizeof(LoRaMeteringParam_t);
}

static int insert_meteringTime(LoRaTimeStamp_t *p, BOOL isJoinMsg)
{
	MeterUnitData_t *pUnit;
	if (isJoinMsg) {
		pUnit = METER_getTempData();
	} else {
		pUnit = METER_getLastData();
	}

	if (pUnit->isTimeSync) {
		p->year = pUnit->year;
		p->mon = pUnit->mon;
		p->day = pUnit->day;
		p->hour = pUnit->hour;
		p->min = pUnit->min;
		p->sec = pUnit->sec;
		printf_ts("MODEM : Metering time - set from data\n");
	} else {
		// 검침 데이터의 시간이 동기화된 것이 아닐 경우 전송시간에 대한 차이값으로 설정.
		if (isJoinMsg) {
			// Join message 의경우 검침 직후 보고하므로 차이값을 0로 설정.
			p->year = 0xFF;
			p->mon = 0xFF;
			p->day = 0xFF;
			p->hour = 0;
			p->min = 0;
			p->sec = 0;
			printf_ts("MODEM : Metering time - set now\n");
		} else {
			// Join message가 아닐 경우 전송시간에 대한 차이값을 계산하여 설정.
			p->year = 0xFF;
			p->mon = 0xFF;
			p->day = 0xFF;

			Date_t now;
			RTC_read(&now);
			uint8 diff = conf.reportInterval % conf.meterInterval;
			// 1. 주기보고의 경우 Base time 및 interval로 계산된 시간을 설정한다.
			// 1.1. 이때 분단위 주기 동작 여부에 따라 다르게 설정한다.
			// 2. 시간단위 주기동작 에서 검침시간 대비 보고시간은 report min만큼 이후에 보고한다.
			// 2.1. 위의 이유로 분단위는 report min 값을 설정한다.
			if (conf.isShortInterval) {
				p->hour = 0;
				p->min = 0;
			} else {
				p->hour = diff;
				p->min = conf.reportMin;
			}
			p->sec = 0;
			printf_ts("MODEM : Metering time - set difference\n");
		}
	}

	return sizeof(LoRaTimeStamp_t);
}

static int insert_tempData(uchar *p)
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

static int insert_multiData(LoRaMeterData_t *p)
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

int MODEM_sendJoin(char *msg)
{
	LoraJoin_t *p = (LoraJoin_t *)msg;

	p->protocol = PROTOCOL_VERSION;

	int len = 0;
	p->mtype = LORA_JOIN;
	len++;

	len += insert_radioQuality(&p->radio);
	len += insert_termInfo(&p->term);
	len += insert_meterInfo(&p->meter);
	len += insert_meteringParam(&p->mp);
	len += insert_meteringTime(&p->meterTime, TRUE);
	len += insert_tempData(p->data);
	len += insert_reset(&p->reset);

	p->len = len;
	p->checksum = cal_checksum(&p->mtype, len);

	return len + 3;
}

int MODEM_sendDataReport(char *msg)
{
	LoraDataReport_t *p = (LoraDataReport_t *)msg;

	p->protocol = PROTOCOL_VERSION;

	int len = 0;
	p->mtype = LORA_DATA_REPORT;
	len++;

	len += insert_radioQuality(&p->radio);
	len += insert_termInfo(&p->term);
	len += insert_meterInfo(&p->meter);
	len += insert_meteringParam(&p->mp);
	len += insert_meteringTime(&p->meterTime, FALSE);
	len += insert_multiData(&p->data);

	p->len = len;
	msg[len + 2] = cal_checksum(&p->mtype, len);

	return len + 3; // 3- protocol, len, checksum
}

void MODEM_rcvDeviceControl(uchar *rxData, int rxLen)
{
	LoraDeviceControl_t *pMsg = (LoraDeviceControl_t *)rxData;

	if (pMsg->protocol == 0xA1 && pMsg->mtype == LORA_DEVICE_CONTROL) {
		uchar checksum = cal_checksum(&pMsg->mtype, pMsg->len);
		if (checksum == pMsg->checksum) {
			uchar mi = pMsg->mi;
			uchar ri = pMsg->ri;

			do {
				if (mi == conf.meterInterval && ri == conf.reportInterval) {
					// 기존 것과 같으면 무시
					break;
				}

				if (ri == 0 || mi == 0 || ri < mi) {
					// 둘 중 하나가 0이거나, 보고주기보다 검침주기보가 크면 무시
					break;
				}

				if ((24 % mi) || (24 % ri)) {
					// 검침주기나 보고주기가 24의 약수가 아니면 무시
					break;
				}

				conf.meterInterval = mi;
				conf.reportInterval = ri;

				FLASH_saveConfigInfo(&conf);
				FLASH_readConfigInfo(&conf);

				printf("Metering paramater changed - mi(%d), ri(%d)\n",
				       conf.meterInterval, conf.reportInterval);
			} while (0);

		} else {
			printf("checksum error - rChecksum(%02x), cChecksum(%02x)\n",
			       pMsg->checksum, checksum);
		}
	} else {
		printf("unknown message from thingPlug - protocol(%02x), mtype(%02x)\n",
		       pMsg->protocol, pMsg->mtype);
	}
}

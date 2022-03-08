#ifndef TDD_TEST
#include <msp430.h>
#include "port_desc.h"
#include "osal_Timer.h"
#endif
#include "battery.h"
#include <ctype.h>
#include <time.h>
#include <stdlib.h>

#include "common_header.h"
#include "uart.h"
#include "check_meter_misc.h"
#include "flashDriver.h"

#include "app.h"
#include "meter.h"
#include "lcdDriver.h"
#include "shell.h"
#include "rtcAlarm.h"
#include "modem.h"
#include "nbiotModem.h"
#include "message.h"
#include "test.h"
#include "tdd.h"

extern uint8 AppProcess;
extern Config_t conf;
extern Modem_t modem;
#ifndef TDD_TEST

static uchar cal_checksum(uchar *p, int len)
{
	uchar checksum = 0;

	for (int i = 0; i < len; i++) {
		checksum += *p++;
	}

	return checksum;
}

static int insert_id(MobileId_t *p)
{
	MODEM_copyImei(p->imei);
	MODEM_copyImsi(p->imsi);

	return sizeof(MobileId_t);
}

static void printMessage(uchar *p, int len)
{
	printf("DL Message - %d byte\n", len);
	for (int i = 0; i < len; i++) {
		printf("%02X ", *(p + i));
	}
	printf("\n");
}

static int insert_termInfo(NbiotDeviceInfo_t *p)
{
	for (int i = 0; i < SERIAL_NUM_LEN / 2; i++) {
		p->termSerial[i] =
			ascii2BCD(conf.serialNum[2 + i * 2 + 0], conf.serialNum[2 + i * 2 + 1]);
	}

	// f/w version
	char fw_ver[0x10] = "";
	strcpy(fw_ver, FIRMWARE_VER);
	fw_ver[0] = '0';

	p->fwVer[0] = ascii2BCD(fw_ver[0], fw_ver[1]);
	p->fwVer[1] = ascii2BCD(fw_ver[2], fw_ver[3]);

#define LOW_BATTERY_THRESHOLD 31
	p->termBatt = BATT_getVoltage();
	if (p->termBatt <= LOW_BATTERY_THRESHOLD) {
		p->termBatt |= 0x80;
	}

	return sizeof(NbiotDeviceInfo_t);
}

static int insert_radioQuality(NbiotRadioQuality_t *p)
{
	int rssi = MODEM_getLastRssi();
	int rsrp = MODEM_getLastRsrp();
	int rsrq = MODEM_getLastRsrq();
	int snr = MODEM_getLastSNR();

	p->rssi = rssi;
	p->ber = 0; // BER - 사용하지 않음.
	memcpy(p->cid, &modem.modemQuality.cid, 2);
	memcpy(p->rsrp, &rsrp, 2);
	memcpy(p->rsrq, &rsrq, 2);
	memcpy(p->snr, &snr, 2);

	return sizeof(NbiotRadioQuality_t);
}

static int insert_meteringParam(NbiotMeteringParam_t *p)
{
	p->mi = conf.meterInterval;
	p->ri = conf.reportInterval;

	return sizeof(NbiotMeteringParam_t);
}

static int insert_meterInfo(NbiotMeterInfo_t *p)
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
		p->meterStatus = pUnitData->meterStatus;
	} else {
		int nData = METER_getNumberOfStoredData();

		for (int i = 0; i < nData; i++) {
			p->meterStatus = METER_getMeterStatus(i);
			if (p->meterStatus != 0xff) {
				break;
			}
		}
	}

	if (p->meterStatus == 0xff) {
		p->meterStatus = 0;
	}

	return sizeof(NbiotMeterInfo_t);
}

static int insert_meteringTime(NbiotTimeStamp_t *p, BOOL isJoinMsg)
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

	return sizeof(NbiotTimeStamp_t);
}

#if 0
static int insert_reset(NbiotReset_t *p)
{
	p->resetCause = 0;
	memcpy(p->resetCount, &conf.resetCount, 2);

	return sizeof(NbiotReset_t);
}
#endif

static int insert_multiData(NbiotMeterData_t *p)
{
	METER_collectStoredData();

	int nData = METER_getNumberOfStoredData();
	if (nData > MAX_NUM_STORED_DATA) {
		nData = MAX_NUM_STORED_DATA;
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
					uint16 diff = (uint16)(prevValue - currValue);
					memcpy(p->valueDiff[i], &diff, 2);
					prevValue = currValue;
				}
			}
		}
	}

	return 7 + (nData * 2); // 7 = interval(1), nData(1), validPos(1), refValue(4)
}

static int insert_oneData(NbiotMeterData_t *p)
{
	p->interval = METER_getSaveInterval();
	p->numData = 1;

	MeterUnitData_t *pUnitData = METER_getTempData();
	if (METER_isAllFF(pUnitData->meterData, 4)) { // meter down
		p->refValuePos = 0xFF;
		memset(p->refValue, 0xFF, 4);
		memset(p->valueDiff[0], 0xFF, 2);
	} else {
		p->refValuePos = 0;

		uint32 value = 0;
		bcd2int(pUnitData->meterData, &value, 4);
		memcpy(p->refValue, &value, 4);
		memset(p->valueDiff[0], 0, 2);
	}

	return 9; // 9 = interval(1), nData(1), validPos(1), refValue(4), diff(2)
}

#if 0
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
#endif

int MODEM_errData(uchar *buf, int pfUlCnt, int pfDlCnt, int udpUlCnt, int udpDlCnt)
{
	uint16 temp16 = 0;
	uint32 temp32 = 0;

	NbiotErrorReportToLg_t *p = (NbiotErrorReportToLg_t *)buf;
	memset(buf, 0, sizeof(NbiotErrorReportToLg_t));

	// Set message version
	p->msgVer = NBIOT_ERR_MSG_VER;

	// Set CTN
	MODEM_copyCtn(p->ctn);

	Date_t date;
	RTC_read(&date);
	temp32 = date.year - 2000;
	int2bcd(&temp32, &p->sendTime[0], 1);
	temp32 = date.mon;
	int2bcd(&temp32, &p->sendTime[1], 1);
	temp32 = date.day;
	int2bcd(&temp32, &p->sendTime[2], 1);
	temp32 = date.hour;
	int2bcd(&temp32, &p->sendTime[3], 1);
	temp32 = date.min;
	int2bcd(&temp32, &p->sendTime[4], 1);
	temp32 = date.sec;
	int2bcd(&temp32, &p->sendTime[5], 1);

	p->ueType = NBIOT_ERR_CURRENT_UE_TYPE;

	p->periodLength = LEN_NBIOT_ERR_P_TIME + LEN_NBIOT_ERR_P_PAYLOAD;
	temp16 = SWAP16(conf.reportInterval * 60);
	memcpy(p->periodTime, &temp16, LEN_NBIOT_ERR_P_TIME);
	temp16 = SWAP16((uint16)LEN_MAX_NBIOT_DATA);
	memcpy(p->periodPayload, &temp16, LEN_NBIOT_ERR_P_PAYLOAD);

	temp16 = SWAP16(pfUlCnt);
	memcpy(p->pfUlCnt, &temp16, LEN_NBIOT_ERR_UL_DL_CNT);
	temp16 = SWAP16(pfDlCnt);
	memcpy(p->pfDlCnt, &temp16, LEN_NBIOT_ERR_UL_DL_CNT);
	temp16 = SWAP16(udpUlCnt);
	memcpy(p->udpUlCnt, &temp16, LEN_NBIOT_ERR_UL_DL_CNT);
	temp16 = SWAP16(udpDlCnt);
	memcpy(p->udpDlCnt, &temp16, LEN_NBIOT_ERR_UL_DL_CNT);

	p->npFlag = 0;

	int logCnt = 0;
	for (int i = 0; i < CNT_NBIOT_ERR_E_LOG; i++) {
		if (modemCtx.errLog[i][0] > 0) {
			logCnt++;
		}
	}

	int dataLen =
		sizeof(NbiotErrorReportToLg_t) + (logCnt * LEN_NBIOT_ERR_E_LOG) + 1; // +1 is EoF
	uchar *pLog = buf + sizeof(NbiotErrorReportToLg_t);
	if (logCnt > 0) {
		temp16 = SWAP16(logCnt * LEN_NBIOT_ERR_E_LOG);
		memcpy(p->errLength, &temp16, LEN_NBIOT_ERR_E_LENGTH);

		for (int i = 0; i < logCnt; i++) {
			memcpy(pLog, modemCtx.errLog[i], LEN_NBIOT_ERR_E_LOG);
			pLog += LEN_NBIOT_ERR_E_LOG;
		}
	}
	*pLog = 0xFF; // set EoF

	return dataLen;
}

#if 0 // sholee - 더 이상 사용하지 않음
int MODEM_sendJoin(uchar *buf)
{
	NbiotJoin_t *p = (NbiotJoin_t *)buf;

	p->protocol = PROTOCOL_VERSION;

	int len = 0;
	p->mtype = NBIOT_JOIN;
	len++;

	len += insert_id(&p->id);
	len += insert_radioQuality(&p->radio);
	len += insert_termInfo(&p->term);
	len += insert_meterInfo(&p->meter);
	len += insert_meteringParam(&p->mp);
	len += insert_meteringTime(&p->meterTime, TRUE);
	len += insert_tempData(p->data);
	len += insert_reset(&p->reset);

	p->len = len;
	p->checksum = cal_checksum(&p->mtype, len);

	return sizeof(NbiotJoin_t);
}
#endif

int MODEM_sendCurrentData(uchar *buf)
{
	NbiotDataReport_t *p = (NbiotDataReport_t *)buf;

	p->protocol = PROTOCOL_VERSION_A3;
	int len = 0;
	p->mtype = NBIOT_DATA_REPORT;
	len++;

	len += insert_id(&p->id);
	len += insert_radioQuality(&p->radio);
	len += insert_termInfo(&p->term);
	len += insert_meterInfo(&p->meter);
	len += insert_meteringParam(&p->mp);
	// Join message는 아니지만 즉시검침/보고를 위한 message이므로
	// Join message와 동일한 방식의 metering time을 얻는다.
	len += insert_meteringTime(&p->meterTime, TRUE);
	len += insert_oneData(&p->data);

	p->len = len;
	buf[len + 2] = cal_checksum(&p->mtype, len); // 2 - protocol, len

	return len + 3; // 3 = protocol + len + checksum
}

int MODEM_sendPeriodicData(uchar *buf)
{
	NbiotDataReport_t *p = (NbiotDataReport_t *)buf;

	p->protocol = PROTOCOL_VERSION_A3;

	int len = 0;
	p->mtype = NBIOT_DATA_REPORT;
	len++;

	len += insert_id(&p->id);
	len += insert_radioQuality(&p->radio);
	len += insert_termInfo(&p->term);
	len += insert_meterInfo(&p->meter);
	len += insert_meteringParam(&p->mp);
	len += insert_meteringTime(&p->meterTime, FALSE);
	len += insert_multiData(&p->data);

	p->len = len;
	buf[len + 2] = cal_checksum(&p->mtype, len); // 2 - protocol, len

	return len + 3; // 3 = protocol + len + checksum
}

int MODEM_recvRepRange(uchar *buf, int len)
{
	NbiotRepRange_t *p = (NbiotRepRange_t *)buf;

	if (p->range == conf.reportRange) {
		printf_ts("DL : report range(%d) is same as current value\n", conf.reportRange);
		return 0;
	}

	if (p->range == 0 || p->range > conf.reportInterval) {
		printf_ts("DL : invalid report range(%d) - not changed\n", conf.reportRange);
		return 0;
	}

	printf_ts("DL : report range changed (%d --> %d)\n", conf.reportRange, p->range);

	conf.reportRange = p->range;

	FLASH_saveConfigInfo(&conf);
	FLASH_readConfigInfo(&conf);

	return 1;
}

int MODEM_recvIntervalReq(uchar *buf, int len)
{
	NbiotIntervalReq_t *p = (NbiotIntervalReq_t *)buf;

	uchar mi = p->mi;
	uchar ri = p->ri;

	// 기존 것과 같으면 무시
	if (mi == conf.meterInterval && ri == conf.reportInterval) {
		printf_ts("DL : interval is same (mi : %d, ri : %d)\n", mi, ri);
		return 0;
	}

	// 둘 중 하나가 0이거나, 보고주기보다 검침주기보가 크면 무시
	if (ri == 0 || mi == 0 || ri < mi) {
		printf_ts("DL : interval is '0' or 'ri < mi' (mi : %d, ri : %d)\n", mi, ri);
		return 0;
	}

	// 검침주기나 보고주기가 24의 약수가 아니면 무시
	if ((24 % mi) || (24 % ri)) {
		printf_ts("DL : interval can not over 24h (mi : %d, ri : %d)\n", mi, ri);
		return 0;
	}

	printf_ts("DL : meteringParameter changed. mi(%d-->%d) ri(%d->%d)\n", conf.meterInterval,
		  mi, conf.reportInterval, ri);

	conf.meterInterval = mi;
	conf.reportInterval = ri;

	FLASH_saveConfigInfo(&conf);
	FLASH_readConfigInfo(&conf);

	return 1;
}

int MODEM_recvResetReq(uchar *buf, int len)
{
#define RESET_STRING "DevReset"
	if (strncmp((char *)buf, RESET_STRING, strlen(RESET_STRING)) == 0) {
		printf_ts("DL : reset request\n");
		MODEM_detach();
		OSAL_setEvent(AppTaskId, APP_EVENT_REBOOT);

		return 1;
	}
	return 0;
}

int MODEM_recvAckString(uchar *buf, int len)
{
#define ACK_STRING "ACK"
	if (strncmp((char *)buf, ACK_STRING, strlen(ACK_STRING)) == 0) {
		printf_ts("DL : ACK string\n");
		return 1;
	}
	return 0;
}

int MODEM_checkDlMessage(uchar *buf, int len)
{
	printMessage(buf, len);
	int resultOK = 0;
	int resetRequired = 0;

	if (*buf < PROTOCOL_VERSION_A1 || *buf > PROTOCOL_VERSION_A3) {
		if (MODEM_recvAckString(buf, len)) {
			resultOK = 1;
		} else if (MODEM_recvResetReq(buf, len)) {
			resultOK = 1;
			resetRequired = 1;
		} else {
			// 문자열의 경우 정확한 패턴이 아니면 쓰레기일 가능성이 있음
		}
	} else {
		NbiotAck_t *p = (NbiotAck_t *)buf;
		uchar c_checksum = cal_checksum(&p->mtype, p->len);
		uchar r_checksum = buf[p->len + 2];

		if (r_checksum == c_checksum) {
			// frame의 경우 모르는 command라도 checksum을 통과했으면 정상적인
			// 응답으로 처리함 - 추후 DL 메시지가 추가되는 상황에 대응하기 위함.
			resultOK = 1;

			if (p->mtype == NBIOT_ACK) {
				// protocol version에 무관하게 정상 처리함.
				printf_ts("DL : ACK Frame\n");
			} else if (p->mtype == NBIOT_INTERVAL_REQ) {
				resetRequired = MODEM_recvIntervalReq(buf, len);
			} else if (p->mtype == NBIOT_CHANGE_R_RANGE) {
				resetRequired = MODEM_recvRepRange(buf, len);
			} else {
				printf_ts("DL : unknown command(%02X)\n", p->mtype);
			}
		} else {
			printf_ts("DL : checksum error(rcv:%02X, cal:%02X)\n", r_checksum,
				  c_checksum);
		}
	}

	if (resetRequired) {
		printf_ts("the device will be reset after detach N/W.\n");
		MODEM_detach();
		OSAL_setEvent(AppTaskId, APP_EVENT_REBOOT);
	}

	return resultOK;
}
#endif

// TDD_TEST

/**
 * @brief LGU+ 품질리포트 Version 1.75
 * See Detail Description README.md
 * @param buf QA Report Message buf
 *
 * @return msg size
 */
int MODEM_qaData(uchar *buf)
{
	NbiotQaReportToLg_t *p = (NbiotQaReportToLg_t *)buf;
	memset(buf, 0, sizeof(NbiotQaReportToLg_t));

	// Set message version
	p->msgVer = NBIOT_QA_MSG_VER;

	// Set CTN
	MODEM_copyCtn(p->ctn);

	// Set battery voltage (2byte BCD)
	p->batt[0] = NBIOT_QA_BATT_VOLTAGE;
	p->batt[1] = (BATT_getVoltage() / 10);
	p->batt[2] = ((BATT_getVoltage() % 10) << 4);

	// Set serving CID
	uint32 cgi = SWAP32(modem.modemQuality.cgi);
	memcpy(p->cgi, &cgi, LEN_NBIOT_QA_CGI);

	// Set RSRP (2byte BCD)
	uint32 rsrp = abs(modem.modemQuality.rsrp) / 10;
	int2bcd(&rsrp, p->rsrp, LEN_NBIOT_QA_RSRP);

	// Set SNR (1byte 음수/양수 설정, 1byte BCD)
	uint32 snr = abs(modem.modemQuality.snr) / 10;
	p->sinr[0] = (modem.modemQuality.snr > 0) ? 0 : 1;
	int2bcd(&snr, &p->sinr[1], LEN_NBIOT_QA_SINR - 1);

	// Set model name
	char *model = TERM_MODEL_STRING(conf.termModel);
	p->model[0] = TERM_MODEL_STRING_LEN; // sizeof(model);
	memcpy(&p->model[1], model, p->model[0]);

	// Set firmware version
	p->fwVer[0] = LEN_NBIOT_QA_FW_VER - 1;

	// byte format len(20) : len(1), deviceVer(4), '/'(1), modemVer(14)
	char version[LEN_NBIOT_QA_FW_VER] = "";
	snprintf(version, LEN_NBIOT_QA_FW_VER, "%s/%s", FIRMWARE_VER, modem.FwVer);
	memcpy(&p->fwVer[1], version, p->fwVer[0]);

	// Set TX power (1byte 음수/양수 설정, 1byte BCD)
	uint32 txPower = abs(modem.modemQuality.txPower) / 10;
	p->txPower[0] = (modem.modemQuality.txPower > 0) ? 0 : 1;
	int2bcd(&txPower, &p->txPower[1], LEN_NBIOT_QA_TX_POWER - 1);

	// 위치좌표, Neighbor Cell ID 지원안함 PASS

	// UE INFO BAND5(LTE), 고정형
#define UE_INFO_BC95G 0x42
	p->ueInfo = UE_INFO_BC95G + modemCtx.proc.initialReport;

	// PORT INFO 사용안함 PASS

	// Reserved 0xF1F1F1
#define QA_RESERVED 0xF1
	for (int i = 0; i < sizeof(p->reserved); i++) {
		p->reserved[i] = QA_RESERVED;
	}

	return sizeof(NbiotQaReportToLg_t);
}

#ifndef NBIOT_LG_TYPE
#include <msp430.h>
#include <ctype.h>
#include <time.h>
#include <stdlib.h>

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
#include "nbiotModem.h"
#include "message.h"
#include "battery.h"
#include "test.h"

extern uint8 AppProcess;
extern Modem_t modem;
extern Config_t conf;

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

	p->termBatt = BATT_getVoltage();

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
	} else {
		pUnitData = METER_getLastData();
	}
	p->meterStatus = pUnitData->meterStatus;

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

static int insert_reset(NbiotReset_t *p)
{
	p->resetCause = 0;
	memcpy(p->resetCount, &conf.resetCount, 2);

	return sizeof(NbiotReset_t);
}

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
	p->fwVer[0] = FIRMWARE_VER_LEN;
	memcpy(&p->fwVer[1], FIRMWARE_VER, p->fwVer[0]);

	// Set TX power (1byte 음수/양수 설정, 1byte BCD)
	uint32 txPower = abs(modem.modemQuality.txPower) / 10;
	p->txPower[0] = (modem.modemQuality.txPower > 0) ? 0 : 1;
	int2bcd(&txPower, &p->txPower[1], LEN_NBIOT_QA_TX_POWER - 1);

	return sizeof(NbiotQaReportToLg_t);
}

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

int MODEM_sendPeriodicData(uchar *buf)
{
	NbiotDataReport_t *p = (NbiotDataReport_t *)buf;

	p->protocol = PROTOCOL_VERSION;

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

int MODEM_recvAck(char *p, int len)
{
	int valid = 0;

	NbiotAck_t *pAck = (NbiotAck_t *)p;
	if (pAck->mtype == NBIOT_ACK) {
		uchar rChecksum = *(p + pAck->len + 2);
		uchar cChecksum = cal_checksum(&pAck->mtype, pAck->len);

		if (rChecksum != cChecksum) {
			printf_ts("DL : checksum error (read : %x, calc : %x) \n", rChecksum,
				  cChecksum);
			return 0;
		}

		Date_t date;

		date.year = pAck->currTime.year + 2000;
		date.mon = pAck->currTime.mon;
		date.day = pAck->currTime.day;
		date.hour = pAck->currTime.hour;
		date.min = pAck->currTime.min;
		date.sec = pAck->currTime.sec;

		RTC_writeTime(date);

		int needReset = 0;
		int mpChanged = 0;
		uchar mi = 0;
		uchar ri = 0;

		if (pAck->len >= LEN_NBIOT_ACK_DATA) {
			if (pAck->resetCommand) {
				needReset = 1;
			}

			mi = pAck->mi;
			ri = pAck->ri;

			do {
				// 분단위 동작의 경우 주기변경 요청을 적용하지 않는다. (NB-IoT only)
				if (conf.isShortInterval) {
					break;
				}

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
				mpChanged = 1;

			} while (0);
		}

		// Convert IMEI data to string
		char imeiStr[19] = "";
		sprintf(imeiStr, "%02X-%02X%02X%02X-%02X%02X%02X-%X", pAck->id.imei[0],
			pAck->id.imei[1], pAck->id.imei[2], pAck->id.imei[3], pAck->id.imei[4],
			pAck->id.imei[5], pAck->id.imei[6], pAck->id.imei[7] / 0x10);

		// Convert IMSI data to string
		uint8 temp[16];
		for (int i = 0; i < 8; i++) {
			temp[i * 2 + 0] = (pAck->id.imsi[i] / 0x10) + '0';
			temp[i * 2 + 1] = (pAck->id.imsi[i] % 0x10) + '0';
		}

		char imsiStr[18] = "";
		sprintf(imsiStr, "%c%c%c-%c%c-%c%c%c%c%c%c%c%c%c%c", temp[0], temp[1], temp[2],
			temp[3], temp[4], temp[5], temp[6], temp[7], temp[8], temp[9], temp[10],
			temp[11], temp[12], temp[13], temp[14]);

		printf_ts("DL : IMEI(%s) IMSI(%s) RTC(%04d-%02d-%02d %02d:%02d:%02d)", imeiStr,
			  imsiStr, date.year, date.mon, date.day, date.hour, date.min, date.sec);

		if (mpChanged) {
			printf(" RI(%d) MI(%d)", ri, mi);
			needReset = TRUE;
		}

		if (needReset) {
			printf(" %s", needReset ? "-- will be reset after 5 sec" : "");
		}

		printf("\n");

		if (needReset) {
			MODEM_detach();
			OSAL_setEvent(AppTaskId, APP_EVENT_REBOOT);
		}

		valid = 1;
	}

	return valid;
}

int MODEM_recvCmdChangeServer(char *p, int len, char *ip, uint16 *port)
{
	int valid = 0;

	NbiotCmdChgServer_t *pCmd = (NbiotCmdChgServer_t *)p;
	if (pCmd->mtype == NBIOT_CMD_CHG_SERVER) {
		uchar rChecksum = *(p + pCmd->len + 2);
		uchar cChecksum = cal_checksum(&pCmd->mtype, pCmd->len);

		if (rChecksum != cChecksum) {
			printf_ts("DL : checksum error (read : %x, calc : %x) \n", rChecksum,
				  cChecksum);
			return 0;
		}

		if (pCmd->len >= LEN_NBIOT_CMD_CHG_SERVER_DATA) {
			Date_t date;

			date.year = pCmd->currTime.year + 2000;
			date.mon = pCmd->currTime.mon;
			date.day = pCmd->currTime.day;
			date.hour = pCmd->currTime.hour;
			date.min = pCmd->currTime.min;
			date.sec = pCmd->currTime.sec;

			RTC_writeTime(date);

			// Convert IMEI data to string
			char imeiStr[19] = "";
			sprintf(imeiStr, "%02X-%02X%02X%02X-%02X%02X%02X-%X", pCmd->id.imei[0],
				pCmd->id.imei[1], pCmd->id.imei[2], pCmd->id.imei[3],
				pCmd->id.imei[4], pCmd->id.imei[5], pCmd->id.imei[6],
				pCmd->id.imei[7] / 0x10);

			// Convert IMSI data to string
			uint8 temp[16];
			for (int i = 0; i < 8; i++) {
				temp[i * 2 + 0] = (pCmd->id.imsi[i] / 0x10) + '0';
				temp[i * 2 + 1] = (pCmd->id.imsi[i] % 0x10) + '0';
			}

			char imsiStr[18] = "";
			sprintf(imsiStr, "%c%c%c-%c%c-%c%c%c%c%c%c%c%c%c%c", temp[0], temp[1],
				temp[2], temp[3], temp[4], temp[5], temp[6], temp[7], temp[8],
				temp[9], temp[10], temp[11], temp[12], temp[13], temp[14]);

			printf_ts("DL : IMEI(%s) IMSI(%s) RTC(%04d-%02d-%02d %02d:%02d:%02d)",
				  imeiStr, imsiStr, date.year, date.mon, date.day, date.hour,
				  date.min, date.sec);

			// Copy requested IP & Port
			sprintf(ip, "%d.%d.%d.%d", pCmd->ip[3], pCmd->ip[2], pCmd->ip[1],
				pCmd->ip[0]);
			memcpy(port, pCmd->port, 2);

			if (strncmp(ip, conf.serverIp, SERVER_IP_STR_LEN) != 0 ||
			    *port != conf.serverPort) {
				printf(" change server(%s:%d)\n", ip, *port);
				valid = 1;
			} else {
				printf(" same server(%s:%d)\n", ip, *port);
				valid = 0;
			}
		}
	}

	return valid;
}

int MODEM_recvIntervalReq(uchar *buf, int len)
{
	if (len <= 0)
		return 0;

	char dl_data[50];
	memset(dl_data, 0, sizeof(dl_data));

	if (*buf != PROTOCOL_VERSION) {
		int rawDataLen = (len / 2); // len이 홀수 인 경우 마지막 1 byte는 버림.
		for (int i = 0; i < rawDataLen; i++) {
			*(dl_data + i) = ascii2BCD(*(buf + i * 2), *(buf + 1 + i * 2));
		}
	} else {
		memcpy(dl_data, buf, sizeof(len));
	}

	NbiotIntervalReq_t *pIntervalReq = (NbiotIntervalReq_t *)dl_data;
	if (pIntervalReq->mtype == NBIOT_INTERVAL_REQ) {
		uchar rChecksum = *(dl_data + pIntervalReq->len + 2);
		uchar cChecksum = cal_checksum(&pIntervalReq->mtype, pIntervalReq->len);

		if (rChecksum != cChecksum) {
			printf_ts("DL : checksum error (read : %x, calc : %x) \n", rChecksum,
				  cChecksum);
			return 0;
		}

		uchar mi = pIntervalReq->mi;
		uchar ri = pIntervalReq->ri;

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

		conf.meterInterval = mi;
		conf.reportInterval = ri;

		FLASH_saveConfigInfo(&conf);
		FLASH_readConfigInfo(&conf);

		printf_ts("DL : meter interval(%d) report interval(%d) -- changed\n", mi, ri);

		return 1;
	}

	return 0;
}

int MODEM_recvResetReq(uchar *buf, int len)
{
	NbiotResetReq_t *pResetReq = (NbiotResetReq_t *)buf;
	if (strstr(pResetReq->msg, "DevReset")) {
		printf_ts("DL : reset request -- will be reset after detach N/W.\n");

		MODEM_detach();
		OSAL_setEvent(AppTaskId, APP_EVENT_REBOOT);

		return 1;
	}

	return 0;
}
#endif

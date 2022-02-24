#include "meter.h"
#include "common_header.h"
#include "uart.h"
#include "check_meter_misc.h"
#include "RTC.h"
#include "MSP430FlashUtil.h"

#include "lcdDriver.h"
#include "battery.h"

#include "Task_Mgr.h"

#include "app.h"
#include "flashDriver.h"
#include "rtcAlarm.h"
#include "nfcProtocol.h"
#include "tdd.h"

#ifndef TDD_TEST
#include <msp430.h>
#include "port_desc.h"
#include "osal_Timer.h"
#define METER_INTERRUPT_HIGH()                                                                     \
	do {                                                                                       \
		P10SEL &= ~0x10, P10DIR |= 0x10, P10OUT |= 0x10;                                   \
	} while (0);
#define METER_INTERRUPT_LOW()                                                                      \
	do {                                                                                       \
		P10SEL &= ~0x10;                                                                   \
		P10DIR |= 0x10;                                                                    \
		P10OUT &= ~0x10;                                                                   \
	} while (0);
#define METER_RXD_PULL_DOWN()                                                                      \
	do {                                                                                       \
		P10REN |= 0x20;                                                                    \
		P10OUT &= ~0x20;                                                                   \
	} while (0);
#define METER_RXD_PULL_UP()                                                                        \
	do {                                                                                       \
		P10REN |= 0x20;                                                                    \
		P10OUT |= 0x20;                                                                    \
	} while (0);
#endif

// 리셋 직후 또는 자석을 댔을 때 읽어온 데이터 저장 영역
MeterTempData_t TempMeterData;

//주기 검침 데이터 저장 영역
MeterStoredData_t StoredMeterData;

inline void insertDateToData(Date_t *pDate, MeterUnitData_t *pData, BOOL isIgnoreSec)
{
	if (RTC_isValidDate(pDate)) {
		// Data의 year는 RTC의 year에서 2000을 빼서 사용
		pData->year = pDate->year - 2000;
		pData->mon = pDate->mon;
		pData->day = pDate->day;
		pData->hour = pDate->hour;
		pData->min = pDate->min;
		pData->sec = (isIgnoreSec == TRUE) ? 0 : pDate->sec;
		pData->isTimeSync = RTC_isTimeSync();
	}
}

inline void copyDateFromData(MeterUnitData_t *pData, Date_t *pDate, BOOL isIgnoreSec)
{
	// RTC의 year는 Data의 year에서 2000을 더해야 함.
	pDate->year = pData->year + 2000;
	pDate->mon = pData->mon;
	pDate->day = pData->day;
	pDate->hour = pData->hour;
	pDate->min = pData->min;
	pDate->sec = (isIgnoreSec == TRUE) ? 0 : pData->sec;
}

static int checkInterval(MeterUnitData_t *after, MeterUnitData_t *before, int si)
{
	Date_t prev;
	copyDateFromData(before, &prev, TRUE);

	Date_t next;
	copyDateFromData(after, &next, TRUE);

	int elapsed = 0;
	if (conf.isShortInterval) {
		if (RTC_calcMinDiff(&prev, &next) >= si) {
			elapsed = 1;
		}
	} else {
		int hourDiff = RTC_calcHourDiff(&prev, &next);
		if (hourDiff >= si) {
			elapsed = 1;
		}
	}

	return elapsed;
}

#ifndef TDD_TEST
char *METER_getMeterName(int meterType)
{
	char *name = "Unknown";
	switch (meterType) {
	case W_STANDARD_D:
		name = "W_STD";
		break;
	case W_SHINHAN_D:
		name = "W_SH";
		break;
	case W_SHINHAN_D_BIG:
		name = "W_SH_B";
		break;
	case W_MNS_D:
		name = "W_MNS";
		break;
	case W_ONETL_D:
		name = "W_ONETL";
		break;
	case G_STANDARD_D:
		name = "G_STD";
		break;
	case G_ONETL_D:
		name = "G_ONETL";
		break;
	case C_STANDARD_D:
		name = "C_STD";
		break;
	case C_ONETL_D:
		name = "C_ONETL";
		break;
	case H_STANDARD_D:
		name = "HW_STD";
		break;
	case H_ONETL_D:
		name = "HW_ONETL";
		break;
	}
	return name;
}

void METER_displayStoredData()
{
	char termSerial[0x10] = "";
	strncpy(termSerial, (char *)conf.serialNum, SERIAL_NUM_LEN);
	termSerial[SERIAL_NUM_LEN] = '\0';

	MeterStoredData_t *p = &StoredMeterData;

	printf("# %d data, serial no: %02x%02x%02x%02x(%02x) T: %s\n", p->nData, p->meterSerial[0],
	       p->meterSerial[1], p->meterSerial[2], p->meterSerial[3], p->caliberDp, termSerial);

	for (int i = 0; i < p->nData; i++) {
		MeterUnitData_t *pUnit = &p->unit[i];
		printf("   %2d)  %2d-%02d-%02d %02d:%02d:%02d   %02x%02x%02x%02x(%02x)\n", i + 1,
		       pUnit->year, pUnit->mon, pUnit->day, pUnit->hour, pUnit->min, pUnit->sec,
		       pUnit->meterData[0], pUnit->meterData[1], pUnit->meterData[2],
		       pUnit->meterData[3], pUnit->meterStatus);
	}
}

void METER_displayTempData()
{
	char termSerial[0x10] = "";
	strncpy(termSerial, (char *)conf.serialNum, SERIAL_NUM_LEN);
	termSerial[SERIAL_NUM_LEN] = '\0';

	MeterStoredData_t *p = &StoredMeterData;

	printf("# serial no: %02x%02x%02x%02x(%02x) T: %s\n", p->meterSerial[0], p->meterSerial[1],
	       p->meterSerial[2], p->meterSerial[3], p->caliberDp, termSerial);

	MeterUnitData_t *pUnit = &TempMeterData.unit;
	printf("         %2d-%02d-%02d %02d:%02d:%02d   %02x%02x%02x%02x(%02x)\n", pUnit->year,
	       pUnit->mon, pUnit->day, pUnit->hour, pUnit->min, pUnit->sec, pUnit->meterData[0],
	       pUnit->meterData[1], pUnit->meterData[2], pUnit->meterData[3], pUnit->meterStatus);
}

BOOL METER_isAllFF(uchar *p, int len)
{
	BOOL result = TRUE;

	for (int i = 0; i < len; i++) {
		if (*(p + i) != 0xFF) {
			result = FALSE;
			break;
		}
	}
	return result;
}

void METER_deleteTempData()
{
	memset(&TempMeterData, 0, sizeof(TempMeterData));
}

void METER_deleteStoredData()
{
	memset(&StoredMeterData, 0, sizeof(StoredMeterData));
	StoredMeterData.saveInterval = conf.meterInterval;
	memset(&StoredMeterData.lastDiffUnit, 0xFF, sizeof(MeterUnitData_t));
}

void METER_deleteAllData()
{
	METER_deleteTempData();
	METER_deleteStoredData();
}

int METER_getNumberOfStoredData()
{
	return StoredMeterData.nData;
}

int METER_getSaveInterval()
{
	return StoredMeterData.saveInterval;
}

void METER_copySerialNum(uint8 *pSerial)
{
	memcpy(pSerial, StoredMeterData.meterSerial, 4);
}

uint8 *METER_getSerialNum()
{
	return StoredMeterData.meterSerial;
}

uint8 METER_getMeterCaliber_dp()
{
	return StoredMeterData.caliberDp;
}

void METER_getMeterData(int index, uint8 *buf)
{
	int nData = METER_getNumberOfStoredData();
	if (index >= 0 && index < nData) {
		memcpy(buf, StoredMeterData.unit[index].meterData, 4);
	}
}

uint8 METER_getMeterStatus(int index)
{
	int nData = METER_getNumberOfStoredData();
	if (index >= 0 && index < nData) {
		return StoredMeterData.unit[index].meterStatus;
	}
	return 0;
}

MeterUnitData_t *METER_getTempData()
{
	return &TempMeterData.unit;
}

MeterUnitData_t *METER_getLastData()
{
	return &StoredMeterData.unit[0];
}

int METER_needNewMetering(int secLimit)
{
	Date_t prev;
	copyDateFromData(&TempMeterData.unit, &prev, FALSE);

	Date_t now;
	RTC_read(&now);

	int32 secDiff = RTC_calcSecDiff(&prev, &now);

	int needMetering = 0;
	if (secDiff < 0 || secDiff > secLimit) {
		needMetering = 1;
	}

#if 0
    printf("prev [%04d-%02d-%02d %02d:%02d:%02d] now[%04d-%02d-%02d %02d:%02d:%02d] diff(%ld) needMetering:%d\n",
                prev.year, prev.mon, prev.day, prev.hour, prev.min, prev.sec,
                now.year, now.mon, now.day, now.hour, now.min, now.sec,
                secDiff, needMetering);
#endif

	return needMetering;
}

MeterUnitData_t *METER_getStoredData(int index)
{
	int nData = METER_getNumberOfStoredData();
	if (index >= 0 && index < nData) {
		return &StoredMeterData.unit[index];
	}
	return NULL;
}

/**
 * @brief Save meter data in Flash with date
 *
 * @param pDate meter data time
 * @param pUnit meter data
 */
void METER_saveInFlash(Date_t *pDate, MeterUnitData_t *pUnit)
{
	// If RTC is not synchronized, do not save meter data in flash
	if (RTC_isTimeSync()) {
		printf("Meter Data Store in Flash : %04d-%02d-%02d %02dH\n", pDate->year,
		       pDate->mon, pDate->day, pDate->hour);
	} else {
		printf("Meter Data Store in Flash : Not synchronized RTC Time\n");
	}
}

void METER_collectStoredData()
{
#ifndef AUX_REPEATER
	uint32 prevMeter;
	uint32 nextMeter;
	uint32 currentMeter;
	MeterUnitData_t *prev;
	MeterUnitData_t *next;
	MeterUnitData_t *current;

	MeterStoredData_t *p = &StoredMeterData;
	if (p->nData <= 0) {
		return;
	}

	MeterUnitData_t lastDiff;
	// Oldest data에 대한 collect를 위해 Last diff data를 복사.
	memcpy(&lastDiff, &p->lastDiffUnit, sizeof(MeterUnitData_t));

	// MT_DOWN meter data collection
	int count = 0;
	const int end = p->nData - 1;
	for (int index = end; index >= 0; index--) {
		current = &p->unit[index];
		next = (index == 0) ? NULL : &p->unit[index - 1];
		prev = (index == end) ? &lastDiff : &p->unit[index + 1];

		if (METER_isAllFF(current->meterData, 4)) {
			if (next == NULL) {
				// Last 값이 MT DOWN 인 경우 이전 검침 값을 copy
				if (METER_isAllFF(prev->meterData, 4) == FALSE) {
					bcd2int(prev->meterData, &prevMeter, 4);
					currentMeter = prevMeter;

					// Meter data, Meter status 복사
					int2bcd(&currentMeter, current->meterData, 4);
					current->meterStatus = prev->meterStatus;
					count++;
				}
			} else {
				// 현재 값이 MT DOWN 이지만, 이전 값과 다음 값이 유효할 경우.
				if ((METER_isAllFF(next->meterData, 4) == FALSE) &&
				    (METER_isAllFF(prev->meterData, 4) == FALSE)) {
					bcd2int(prev->meterData, &prevMeter, 4);
					bcd2int(next->meterData, &nextMeter, 4);
					if (prevMeter > nextMeter) {
						// PREV-NEXT 값이 BACKFLOW인 경우 NEXT 값을 할당.
						currentMeter = nextMeter;
					} else {
						currentMeter = (prevMeter + nextMeter) / 2;
					}

					// Meter data, Meter status 복사
					int2bcd(&currentMeter, current->meterData, 4);
					current->meterStatus = prev->meterStatus;
					count++;
				}
			}
		}
	}

	// Next collect를 위해 Last data를 복사.
	memcpy(&p->lastDiffUnit, &p->unit[0], sizeof(MeterUnitData_t));

	if (count > 0) {
		printf_ts("METER : %d datas are collected\n", count);
	}
#endif
}

void METER_addTempData(Date_t *pDate, MeterUnitData_t *pUnit)
{
	insertDateToData(pDate, pUnit, FALSE);

	memcpy(&TempMeterData.unit, pUnit, sizeof(MeterUnitData_t));
	TempMeterData.caliberDp = StoredMeterData.caliberDp;
	TempMeterData.dif = StoredMeterData.dif;
	TempMeterData.vif = StoredMeterData.vif;
	memcpy(TempMeterData.meterSerial, StoredMeterData.meterSerial, 4);
}

int METER_getFirstValidPos()
{
	int pos = 0xFF;

	for (int i = 0; i < StoredMeterData.nData; i++) {
		if (METER_isAllFF(StoredMeterData.unit[i].meterData, 4) == FALSE) {
			pos = i;
			break;
		}
	}

	return pos;
}

void METER_saveMeterInfo(uint8 *serial, uint8 caliberDp, uint8 dif, uint8 vif)
{
	if (memcmp(serial, StoredMeterData.meterSerial, 4) ||
	    caliberDp != StoredMeterData.caliberDp) {
		// data를 처음 읽은 것이거나 중간에 계량기가 교체된 것임
		METER_clearStoredData();
		memcpy(StoredMeterData.meterSerial, serial, 4);
		StoredMeterData.caliberDp = caliberDp;
		if (dif & vif) {
			StoredMeterData.dif = dif;
			StoredMeterData.vif = vif;
		}
	}
}

uint8 std_checksum(uint8 *from, int size)
{
	uint8 checksum = 0;
	for (int i = 0; i < size; i++) {
		checksum += *(from + i);
	}

	return checksum;
}

BOOL read_std_d_meter(uint8 *rxBuf, MeterUnitData_t *pUnit)
{
	if (rxBuf[0] != STD_RESP_START || rxBuf[3] != STD_RESP_START) {
		printf("invalid message\n");
		return FAIL;
	}

	StdMeterDataResp_t *pMsg = (StdMeterDataResp_t *)rxBuf;

	uint8 cChecksum =
		std_checksum((uint8 *)&pMsg->cfield, (int)(&pMsg->checksum - &pMsg->cfield));
	if (cChecksum != pMsg->checksum) {
		return FAIL;
	}

	for (int i = 0; i < 4; i++) {
		pUnit->meterData[i] = pMsg->value[3 - i];
	}

	pUnit->meterStatus = pMsg->status;
	pUnit->icon.lowBatt = (pUnit->meterStatus & 0x04) ? 1 : 0;
	pUnit->icon.rArrow = (pUnit->meterStatus & 0x40) ? 1 : 0;
	pUnit->icon.fArrow = 0; // always 0
	pUnit->icon.m3 = 1;
	pUnit->icon.notUsed = 0; // 표준 프로토콜에는 미사용 없음.
	pUnit->icon.leak = (pUnit->meterStatus & 0x20) ? 1 : 0;

	uint8 serial[4];
	for (int i = 0; i < 4; i++) { // serial 4byte
		serial[i] = pMsg->serial[3 - i];
	}

	uchar caliberDp = (pMsg->dif & 0xF0) | (pMsg->vif & 0x0F);

	METER_saveMeterInfo(serial, caliberDp, pMsg->dif, pMsg->vif);

	return SUCCESS;
}

// 신한 대용량 계량기는 계량기의 serial no가 6바이트임
// Data format of Shinhan Meter
//  byte 0 - IRQ_ACK(always 0x06)
//  byte 1 - STX(always 0x02)
//  byte 2~14 - data
//      -> byte 2 - length(0x0d = 13)
//      -> byte 3~8 - serial
//      -> byte 9~12- value
//      -> byte 13 - battery
//      -> byte 14 - status
//  byte 15 - ETX(always 0x03)
//  byte 16 - checksum (XORed value of byte 1 ~ 13)

// 신한 소용량 계량기는 계량기의 serial no가 4바이트임
// Data format of Shinhan Meter
//  byte 0 - IRQ_ACK(always 0x06)
//  byte 1 - STX(always 0x02)
//  byte 2~12  - data
//      -> byte 2 - length(0x0b = 11)
//      -> byte 3~6 - serial
//      -> byte 7~10- value
//      -> byte 11 - battery
//      -> byte 12 - status
//  byte 13 - ETX(always 0x03)
//  byte 14 - checksum (XORed value of byte 1 ~ 13)

BOOL read_shinhan_d_meter(uint8 *rxBuf, MeterUnitData_t *pUnit)
{
	uint8 checksum = 0x00;
	int offset = 0;
	int frame_len = 0;

	uchar decimalPoint = 3;
	if (rxBuf[2] == SHINHAN_METER_DATA_LEN) {
		offset = 0;
		frame_len = SHINHAN_METER_FRAME_LEN;
		decimalPoint = 3;
	} else if (rxBuf[2] == SHINHAN_BIG_METER_DATA_LEN) {
		offset = SHINHAN_BIG_METER_OFFSET;
		frame_len = SHINHAN_BIG_METER_FRAME_LEN;
		decimalPoint = 2;
	} else {
		printf("data length mismatched! (len = %d)\n", rxBuf[2]);
		return FAIL;
	}

	if (rxBuf[0] != 0x06 || rxBuf[1] != 0x02 || rxBuf[13 + offset] != 0x03) {
		printf("Shinhan meter - data format mismatched!\n");
		return FAIL;
	}

	checksum = 0;
	for (int i = 1; i < frame_len - 1; i++) {
		checksum ^= rxBuf[i];
	}

	if (checksum != rxBuf[frame_len - 1]) {
		printf("Meter Checksum Error: 0x%x , 0x%x\n", checksum, rxBuf[frame_len - 1]);
		return FAIL;
	}

	int dataValid = 0;
	for (int i = 0; i < 4; i++) {
		dataValid += rxBuf[10 + offset - i];
		pUnit->meterData[i] = rxBuf[10 + offset - i];
	}

#if 0
    if(dataValid == 0) {
        // data가 모두 0이면 잘못된 데이터로 처리함.
        printf("Meter value is 0 - ignored\n");
        return FAIL;
    }
#endif

	pUnit->meterStatus = rxBuf[12 + offset];
	if ((pUnit->meterStatus & 0xC0) == 0xC0) {
		// status의 비트 7, 6, 2, 0에 따라 신한제품과 신한모드 하이텍제품을 구분
		// (이들 비트가 신한은 모두 0, 하이텍은 모두 1)

		pUnit->meterStatus &= ~0xC5; // 수신측에서 동일하게 처리하도록 mask out
	}

	pUnit->icon.lowBatt = rxBuf[11 + offset] < 2 ? 1 : 0;
	pUnit->icon.rArrow = (pUnit->meterStatus & 0x10) ? 1 : 0;
	pUnit->icon.fArrow = 0; // always 0
	pUnit->icon.m3 = 1;
	pUnit->icon.notUsed = (pUnit->meterStatus & 0x20) ? 1 : 0;
	pUnit->icon.leak = (pUnit->meterStatus & 0x08) ? 1 : 0;

	uint8 serial[4];
	for (int i = 0; i < 4; i++) { // serial 4byte
		serial[i] = rxBuf[6 - i];
	}

	// printf("  - Serial Num  : %02x%02x%02x%02x\n", serial[0], serial[1], serial[2], serial[3]);
	// printf("  - Meter Value : %02x%02x%02x%02x (%d)\n",
	//             pUnit->meterData[0], pUnit->meterData[1],
	//             pUnit->meterData[2], pUnit->meterData[3], decimalPoint);
	// printf("  - Status      : %02x\n", pUnit->meterStatus);

	METER_saveMeterInfo(serial, decimalPoint, 0, 0);

	return SUCCESS;
}

uint8 hex2int(uint8 value)
{
	if (value >= '0' && value <= '9') {
		return (value - '0');
	}

	if (value >= 'A' && value <= 'F') {
		return (value - 'A' + 10);
	}

	if (value >= 'a' && value <= 'f') {
		return (value - 'a' + 10);
	}

	return 0xff;
}

BOOL read_mns_d_meter(uint8 *rxBuf, MeterUnitData_t *pUnit)
{
	MnsMeter_t *pRaw = (MnsMeter_t *)rxBuf;

	if (pRaw->startByte1 != '*' || pRaw->startByte2 != '&' || pRaw->carriageReturn1 != 0x0d ||
	    pRaw->carriageReturn2 != 0x0d) {
		printf("MNS Meter - data format mismatched\n");
		return FAIL;
	}

	uint8 rChecksum = hex2int(pRaw->checksum[0]) * 16 + hex2int(pRaw->checksum[1]);
	uint8 cChecksum = 0;

	for (int i = MNS_CHECKSUM_START1; i <= MNS_CHECKSUM_STOP1; i++) {
		cChecksum += rxBuf[i - 1];
	}

	for (int i = MNS_CHECKSUM_START2; i <= MNS_CHECKSUM_STOP2; i += 2) {
		cChecksum += (hex2int(rxBuf[i - 1]) * 16 + hex2int(rxBuf[i]));
	}

	for (int i = MNS_CHECKSUM_START3; i <= MNS_CHECKSUM_STOP3; i += 2) {
		cChecksum += (hex2int(rxBuf[i - 1]) * 16 + hex2int(rxBuf[i]));
	}

	if (rChecksum != cChecksum) {
		printf("M&S Meter - Checksum error[r(%02x) : c(%02x)]\n", rChecksum, cChecksum);
		return FAIL;
	}

	// meter data
	for (int i = 0; i < 4; i++) {
		pUnit->meterData[i] = (pRaw->meterValue[2 * i + 0] - '0') << 4 |
				      pRaw->meterValue[2 * i + 1] - '0';
	}

	uint8 serial[4];
	for (int i = 0; i < 4; i++) { // serial 4byte
		serial[i] = pRaw->userID[2 * i + 2] - '0' << 4 | pRaw->userID[2 * i + 3] - '0';
	}

	pUnit->meterStatus = 0;
	uchar caliberDp = 0x13; // MNS는 항상 15mm/소숫점 3자리

	// mns는 상태정보를 제공하지 않음
	pUnit->icon.lowBatt = 0;
	pUnit->icon.rArrow = 0;
	pUnit->icon.fArrow = 0;
	pUnit->icon.m3 = 1;
	pUnit->icon.notUsed = 0;
	pUnit->icon.leak = 0;

	METER_saveMeterInfo(serial, caliberDp, 0, 0);

	return SUCCESS;
}

/**
 * @brief OneTL meter response parser (Two type of message support)
 *
 * @param rxBuf Read data by UART
 * @param len   Length of read data
 * @param pUnit Meter unit for data delivery
 * @return BOOL TRUE is success parsing, FALSE is fail.
 */
BOOL read_onetl_meter(uint8 *rxBuf, int len, MeterUnitData_t *pUnit)
{
	int stxIdx = 0;
	while (stxIdx < len) {
		if (rxBuf[stxIdx] == ONETL_RESP_STX) {
			break;
		}
		stxIdx++;
	}
	if (stxIdx >= len)
		return FAIL;

	uint8 *pBuf = (rxBuf + stxIdx);
	OntelHeader_t *pMsg = (OntelHeader_t *)pBuf;

	uint8 *msgStx = pBuf;
	uint8 *msgBcc = pBuf + pMsg->len;
	uint8 *msgEtx = pBuf + pMsg->len + 1;
	if (*msgStx != ONETL_RESP_STX || *msgEtx != ONETL_RESP_ETX) {
		printf("invalid message\n");
		return FAIL;
	}

	uint8 bcc = std_checksum(msgStx, (int)(pMsg->len));
	bcc &= ONETL_BCC_AND_CONSTANT;
	if (bcc != *msgBcc) {
		printf("checksum error message\n");
		return FAIL;
	}

	switch (pMsg->id) {
	case ONETL_ID_READ_FLOW: {
		OntelFlowResp_t *pMsg = (OntelFlowResp_t *)pBuf;
		for (int i = 0; i < 4; i++) {
			pUnit->meterData[i] = LO_UINT8(pMsg->meterValue[4 - i]) << 4 |
					      HI_UINT8(pMsg->meterValue[3 - i]);
		}

		pUnit->meterStatus = pMsg->status;
		pUnit->icon.lowBatt = (pUnit->meterStatus & 0x03 == ONETL_STATUS_BATT_FULL) ? 0 : 1;
		// 상태정보의 경우 배터리를 제외하고 없음.
		pUnit->icon.rArrow = 0;
		pUnit->icon.fArrow = 0; // always 0
		pUnit->icon.m3 = 1; // Unit is m3
		pUnit->icon.notUsed = 0;
		pUnit->icon.leak = 0;

		uint8 serial[4];
		for (int i = 0; i < 4; i++) { // serial 4byte
			serial[i] = pMsg->serial[3 - i];
		}

		uchar decimalPoint = 0x03; // 소수점 3자리
		METER_saveMeterInfo(serial, decimalPoint, 0, 0);
	} break;

	case ONETL_ID_READ_TOTAL: {
		OntelTotalResp_t *pMsg = (OntelTotalResp_t *)pBuf;
		for (int i = 0; i < 4; i++) {
			pUnit->meterData[i] = LO_UINT8(pMsg->meterValue[4 - i]) << 4 |
					      HI_UINT8(pMsg->meterValue[3 - i]);
			pUnit->flowData[i] = LO_UINT8(pMsg->flowValue[4 - i]) << 4 |
					     HI_UINT8(pMsg->flowValue[3 - i]);
		}

		for (int i = 0; i < 2; i++) {
			pUnit->st[i] = pMsg->rt[1 - i];
			pUnit->rt[i] = pMsg->rt[1 - i];
		}

		pUnit->meterStatus = pMsg->status;
		pUnit->icon.lowBatt = (pUnit->meterStatus & 0x03 == ONETL_STATUS_BATT_FULL) ? 0 : 1;
		// 상태정보의 경우 배터리를 제외하고 없음.
		pUnit->icon.rArrow = 0;
		pUnit->icon.fArrow = 0; // always 0
		pUnit->icon.m3 = 0; // Unit is MWh
		pUnit->icon.notUsed = 0;
		pUnit->icon.leak = 0;

		uint8 serial[4];
		for (int i = 0; i < 4; i++) { // serial 4byte
			serial[i] = pMsg->serial[3 - i];
		}

		uchar decimalPoint = 0x03; // 소수점 3자리
		METER_saveMeterInfo(serial, decimalPoint, 0, 0);
	} break;

	default:
		printf("READ OTHERS\n");
		return FAIL;
	}

	return SUCCESS;
}

/**
 * @brief Send meter data request to OneTL meter
 *
 * @param meterType Meter type. (support Calori, Water, Hot water, Gas)
 * @return BOOL  TRUE is success sending.
 */
BOOL METER_ontel_sendRequest(uint8 meterType)
{
	LCD_wait();

	OntelReq_t msg;
	OntelReq_t *pMsg = &msg;

	switch (meterType) {
	case C_ONETL_D: {
		pMsg->id = ONETL_ID_READ_TOTAL;
		pMsg->address = ONETL_C_METER_ADDR;
	} break;

	case W_ONETL_D: {
		pMsg->id = ONETL_ID_READ_FLOW;
		pMsg->address = ONETL_W_METER_ADDR;
	} break;

	case H_ONETL_D: {
		pMsg->id = ONETL_ID_READ_FLOW;
		pMsg->address = ONETL_H_METER_ADDR;
	} break;

	case G_ONETL_D: {
		pMsg->id = ONETL_ID_READ_FLOW;
		pMsg->address = ONETL_G_METER_ADDR;
	} break;

	default:
		return FALSE;
	}

	pMsg->stx = ONETL_REQ_STX;
	pMsg->len = sizeof(OntelReq_t) - 2; //-2 is stx and etc
	pMsg->etx = ONETL_REQ_ETX;

	uint8 bcc = std_checksum((uint8 *)&pMsg->stx, (int)pMsg->len);
	bcc &= ONETL_BCC_AND_CONSTANT;
	pMsg->bcc = bcc;

	METER_enable(meterType);
	MISC_delayMs(50); // 문서상으로는 없으나 최초 PULL-UP 후 일정 delay 값이 필요.

	UART_send(UART_A3, &msg, sizeof(OntelReq_t), FALSE);
	return TRUE;
}

BOOL METER_shinhan_sendRequest()
{
	//              +----------+         +----------+
	//              |          |         |          |
	//              |          |         |          |
	//  -------------          -----------          -----------------
	//                  125ms     125ms      125ms
	//

	LCD_wait();

	MISC_delayMs(1);

	METER_INTERRUPT_HIGH();
	MISC_delayMs(125);

	METER_INTERRUPT_LOW();
	MISC_delayMs(125);

	METER_INTERRUPT_HIGH();
	MISC_delayMs(125);

	METER_INTERRUPT_LOW();

	METER_enable(W_SHINHAN_D);

	return TRUE;
}

BOOL METER_mns_sendRequest()
{
	//             +----+
	//             |    |
	//             |    |
	//  ------------    --------------------------------
	//               6ms
	//
	LCD_wait();

	METER_INTERRUPT_LOW();
	METER_INTERRUPT_HIGH();

	MISC_delayMs(6);

	METER_INTERRUPT_LOW();

	METER_enable(W_MNS_D);

	return TRUE;
}

BOOL METER_std_sendRequest()
{
	LCD_wait();

	StdMeterReq_t msg;
	StdMeterReq_t *pMsg = &msg;

	pMsg->stx = STD_REQ_START;
	pMsg->cfield = STD_METER_DATA_REQ;
	pMsg->afield = STD_REQ_ADDR;
	pMsg->checksum =
		std_checksum((uint8 *)&pMsg->cfield, (int)(&pMsg->checksum - &pMsg->cfield));
	pMsg->etx = STD_REQ_STOP;

	METER_enable(W_STANDARD_D);
	MISC_delayMs(20); // STD spec delay for waitting slave prepare

	UART_send(UART_A3, &msg, sizeof(StdMeterReq_t), FALSE);
	return TRUE;
}

BOOL METER_std_lcdTestReq()
{
	LCD_wait();

	StdLcdTestReq_t msg;

	msg.stx = STD_REQ_START;
	msg.cfield = STD_LCD_TEST_REQ;
	msg.afield = STD_REQ_ADDR;
	msg.data = 1; // LCD test always operate to display all items.
	msg.time = 0; // default 2sec
	msg.checksum = std_checksum((uint8 *)&msg.cfield, (int)(&msg.checksum - &msg.cfield));
	msg.etx = STD_REQ_STOP;

	METER_enable(W_STANDARD_D);
	MISC_delayMs(20); // STD spec delay for waitting slave prepare

	UART_send(UART_A3, &msg, sizeof(StdLcdTestReq_t), FALSE);
	return TRUE;
}

BOOL METER_std_lcdMarkReq(BOOL isMarkOn)
{
	LCD_wait();

	StdLcdMarkReq_t msg;

	msg.stx = STD_REQ_START;
	msg.cfield = STD_LCD_MARK_REQ;
	msg.afield = STD_REQ_ADDR;
	msg.data = (isMarkOn == TRUE) ? 1 : 0; // 1 is on H Mark, 0 is off H Mark
	msg.checksum = std_checksum((uint8 *)&msg.cfield, (int)(&msg.checksum - &msg.cfield));
	msg.etx = STD_REQ_STOP;

	METER_enable(W_STANDARD_D);
	MISC_delayMs(20); // STD spec delay for waitting slave prepare

	UART_send(UART_A3, &msg, sizeof(StdLcdTestReq_t), FALSE);
	return TRUE;
}

BOOL METER_std_configRequest()
{
	LCD_wait();

	StdMeterReq_t msg;
	StdMeterReq_t *pMsg = &msg;

	pMsg->stx = STD_REQ_START;
	pMsg->cfield = STD_METER_CONFIG_REQ;
	pMsg->afield = STD_REQ_ADDR;
	pMsg->checksum =
		std_checksum((uint8 *)&pMsg->cfield, (int)(&pMsg->checksum - &pMsg->cfield));
	pMsg->etx = STD_REQ_STOP;

	METER_enable(W_STANDARD_D);
	MISC_delayMs(20); // STD spec delay for waitting slave prepare

	UART_send(UART_A3, &msg, sizeof(StdMeterReq_t), FALSE);
	return TRUE;
}

// Only STD, It is request to adjust Q3 ~ Q1 and Qt
BOOL METER_std_adjustReq(char *sn, uint8 caliber, uint8 maker, uint16 q3, uint16 qt, uint16 q2,
			 uint16 q1)
{
	LCD_wait();

	// printf("Maker/Caliber : %d/%d \n", maker, caliber);
	// printf("Q3/Qt/Q2/Q1   : %d/%d/%d/%d \n", q3, qt, q2, q1);

	StdMeterAdjustReq_t msg;

	memcpy(msg.serial, sn, 4);
	msg.stx = STD_REQ_START;
	msg.cfield = STD_METER_ADJUST_REQ;
	msg.afield = STD_REQ_ADDR;
	msg.caliber = caliber;
	msg.maker = maker;
	msg.q3[0] = q3 & 0x00FF;
	msg.q3[1] = q3 & 0xFF00 >> 8;
	msg.qt[0] = qt & 0x00FF;
	msg.qt[1] = qt & 0xFF00 >> 8;
	msg.q2[0] = q2 & 0x00FF;
	msg.q2[1] = q2 & 0xFF00 >> 8;
	msg.q1[0] = q1 & 0x00FF;
	msg.q1[1] = q1 & 0xFF00 >> 8;
	msg.checksum = std_checksum((uint8 *)&msg.cfield, (int)(&msg.checksum - &msg.cfield));
	msg.etx = STD_REQ_STOP;

	METER_enable(W_STANDARD_D);
	MISC_delayMs(20); // STD spec delay for waitting slave prepare

	UART_send(UART_A3, &msg, sizeof(StdMeterAdjustReq_t), FALSE);
	return TRUE;
}

// Only STD, It is response of adjust request
BOOL METER_std_adjustResp(StdAdjustResult_t *result)
{
	uint8 buf[METER_RX_BUF_LEN];
	memset(buf, 0, METER_RX_BUF_LEN);
	int len = UART_receive(UART_A3, buf, METER_RX_BUF_LEN);

	StdMeterAdjustResp_t *resp = (StdMeterAdjustResp_t *)buf;

	if (resp->stx1 != STD_RESP_START || resp->stx2 != STD_RESP_START) {
		printf("invalid message : start bit is not matched\n");
		return FAIL;
	}

	int checksum = std_checksum((uint8 *)&resp->cfield, (int)(&resp->checksum - &resp->cfield));
	if (resp->checksum != checksum) {
		printf("invalid message : checksum fail\n");
		return FAIL;
	}

	if (resp->cfield == STD_METER_ADJUST_RESP) {
		memcpy(result, (buf + (int)(&(resp->caliber) - &(resp->stx1))),
		       sizeof(StdAdjustResult_t));
	} else {
		printf("invalid message, not matched 'cfield'\n");
	}

	return SUCCESS;
}

/**
 * @brief Send STD request from NFC to meter(STD)
 *
 * @param body      request data
 * @param bodyLen   request data len
 */
void METER_bypassReq(uint8 *body, int bodyLen)
{
	LCD_wait();

	if (bodyLen > METER_TX_BUF_LEN - 2) {
		bodyLen = METER_TX_BUF_LEN - 2;
	}

	uint8 txmsg[METER_TX_BUF_LEN];
	memset(txmsg, 0, METER_TX_BUF_LEN);

	txmsg[0] = STD_REQ_START; // stx
	memcpy((uint8 *)&txmsg[1], body, bodyLen);
	txmsg[bodyLen + 1] = STD_REQ_STOP; // etx

	METER_enable(W_STANDARD_D);
	MISC_delayMs(20); // STD spec delay for waitting slave prepare

	UART_send(UART_A3, txmsg, bodyLen + 2, FALSE);

	//    PRINT_hexBuffer(txmsg, bodyLen+2);
}

BOOL METER_sendRequest(uint8 meterType)
{
	BOOL result = FALSE;

	if (meterType == W_STANDARD_D) {
		result = METER_std_sendRequest();
	} else if (meterType == W_SHINHAN_D || meterType == W_SHINHAN_D_BIG) {
		result = METER_shinhan_sendRequest();
	} else if (meterType == W_MNS_D) {
		result = METER_mns_sendRequest();
	} else if (METER_TYPE_ONETL(meterType)) {
		result = METER_ontel_sendRequest(meterType);
	}
	return result;
}

void METER_enable(uint8 meterType)
{
#if !defined(AUX_REPEATER)
	if (conf.debugPrint == 2) {
		return;
	}

	UartInitParam_t param = {
		.isRxEnable = TRUE,
		.parity = UART_PARITY_NONE,
		.stopbit = UART_STOPBIT_ONE,
		.charLen = UART_8BITS_CHAR,
	};

	if (meterType == W_SHINHAN_D || meterType == W_SHINHAN_D_BIG) {
		param.isTxEnable = FALSE;
		param.baudrate = UART_BAUDRATE_600;
	} else if (METER_TYPE_STANDARD(meterType)) {
		param.isTxEnable = TRUE;
		param.baudrate = UART_BAUDRATE_1200;
	} else if (meterType == W_MNS_D) {
		param.isTxEnable = FALSE;
		param.baudrate = UART_BAUDRATE_1200;
		param.parity = UART_PARITY_EVEN;
		param.stopbit = UART_STOPBIT_ONE;
		param.charLen = UART_7BITS_CHAR;
	} else if (METER_TYPE_ONETL(meterType)) {
		param.isTxEnable = TRUE;
		param.baudrate = UART_BAUDRATE_2400;
	} else {
		return;
	}

	UART_open(UART_A3, &param);
	if (meterType == W_MNS_D || meterType == W_SHINHAN_D || meterType == W_SHINHAN_D_BIG) {
		METER_INTERRUPT_LOW();
		METER_RXD_PULL_DOWN();
		METER_RXD_PULL_UP();
	}
#endif // #if !defined(AUX_REPEATER)
}

void METER_disable()
{
	if (conf.debugPrint == 2) {
		return;
	}

	UART_close(UART_A3, GPIO_LOW);
	METER_INTERRUPT_LOW();
	METER_RXD_PULL_DOWN();
}

BOOL METER_recvResponse(uint8 meterType, MeterUnitData_t *pUnit)
{
	uint8 buf[METER_RX_BUF_LEN];
	memset(buf, 0, METER_RX_BUF_LEN);
	int len = UART_receive(UART_A3, buf, METER_RX_BUF_LEN);

	BOOL result = FALSE;
	if (meterType == W_STANDARD_D) {
		result = read_std_d_meter(buf, pUnit);
	} else if (meterType == W_SHINHAN_D || meterType == W_SHINHAN_D_BIG) {
		result = read_shinhan_d_meter(buf, pUnit);
	} else if (meterType == W_MNS_D) {
		result = read_mns_d_meter(buf, pUnit);
	} else if (METER_TYPE_ONETL(meterType)) {
		result = read_onetl_meter(buf, len, pUnit);
	}

	return result;
}

BOOL METER_bypassResp()
{
	uint8 buf[METER_RX_BUF_LEN];
	memset(buf, 0, METER_RX_BUF_LEN);
	int len = UART_receive(UART_A3, buf, METER_RX_BUF_LEN);

	if (buf[0] != STD_RESP_START || buf[3] != STD_RESP_START) {
		printf("invalid message\n");
		return FAIL;
	}

	// PRINT_hexBuffer(rxMsg, len);
	// 4 - stx1,l1field, l2field, stx2
	// 5 - stx1,l1field, l2field, stx2, etx
	NFCAPP_meterAdjustResp(buf + 4, len - 5);
	return SUCCESS;
}
#endif

// TDD Test

/**
 * @brief Save meter data in RAM memory. (If num of data is max, compress saved datas)
 *
 * @param pDate meter data time
 * @param pUnit meter data
 */
BOOL METER_addStoredData(Date_t *pDate, MeterUnitData_t *pUnit)
{
	int retVal = 0;
	insertDateToData(pDate, pUnit, TRUE);

	MeterStoredData_t *p = &StoredMeterData;
	if (p->saveInterval < 1 || p->saveInterval > 24) {
		p->saveInterval = 1;
	}

	if (p->nData == 0 || checkInterval(pUnit, &p->unit[0], p->saveInterval)) {
#if LORA_DEVICE
		int nMaxData = NUM_LORA_STORED_DATA;
#else // NBIOT_DEVICE
		int nMaxData = NUM_NBIOT_STORED_DATA;
#endif

		if (p->nData >= nMaxData) {
			p->nData = nMaxData;

			if (conf.dataSkipMode) {
				// saveInterval Max 4 Day(수자원 공사 요구사항)
				switch (p->saveInterval) {
				case 1:
				case 2:
					// 하나 걸러 하나씩 없앰 - 짝수 번째 것은 무조건 지우고,
					// 홀수 번째 것은 1->0, 3->1, 5->2, 7->3, 9->4와 같이 이동 후 삭제.
					// 결과적으로 데이터의 갯수는 절반이 됨
					for (int i = 0; i < p->nData; i++) {
						if ((i % 2) == 0) {
							memset(&p->unit[i], 0,
							       sizeof(MeterUnitData_t));
						} else {
							memcpy(&p->unit[(i - 1) / 2], &p->unit[i],
							       sizeof(MeterUnitData_t));
							memset(&p->unit[i], 0,
							       sizeof(MeterUnitData_t));
						}
					}
					p->nData /= 2;
					p->saveInterval *= 2;
					break;

				default:
					p->nData--;
					break;
				}
			} else {
				// 서울시 기본 마지막 데이터만 버림 Save Max : 1Day
				p->nData--;
			}
		}

		if (p->nData) {
			for (int i = p->nData - 1; i >= 0; i--) {
				memcpy(&p->unit[i + 1], &p->unit[i], sizeof(MeterUnitData_t));
			}
		}

		memcpy(&p->unit[0], pUnit, sizeof(MeterUnitData_t));
		p->nData++;
		retVal = TRUE;
	}

	return retVal;
}

/**
 * @brief nData == MAX인 상태에서Uplink는 성공했으나 Downlink는 실패한 경우 실행된다.
 * ReportInterval 만큼의 검침 데이터를 삭제시킨다.
 */
void METER_clearIntervalData()
{
#if LORA_DEVICE
	int nMaxData = NUM_LORA_STORED_DATA;
#else // NBIOT_DEVICE
	int nMaxData = NUM_NBIOT_STORED_DATA;
#endif
	MeterStoredData_t *p = &StoredMeterData;

	if (conf.reportInterval < 1 || conf.reportInterval > 24) {
		conf.reportInterval = 6;
	}
	if (p->nData > nMaxData) {
		p->nData = nMaxData;
	}

	if (p->nData == nMaxData) {
		uint8 interval = conf.reportInterval;
		for (int i = p->nData - 1; i >= p->nData - interval; i--) {
			memset(&p->unit[i], 0, sizeof(MeterUnitData_t));
		}

		p->nData -= interval;
	}
}

void METER_clearStoredData()
{
	StoredMeterData.nData = 0;

	if (conf.meterInterval < 1 || conf.meterInterval > 24) {
		conf.meterInterval = 1;
	}

	StoredMeterData.saveInterval = conf.meterInterval;
	for (int i = 0; i < MAX_NUM_STORED_DATA; i++) {
		memset(&StoredMeterData.unit[i], 0, sizeof(MeterUnitData_t));
	}
}

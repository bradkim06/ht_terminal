#include <msp430.h>
#include <ctype.h>
#include "common_header.h"
#include "MSP430FlashUtil.h"
#include "dataFlash.h"
#include "rtcAlarm.h"
#include "check_meter_misc.h"
#include "flashDriver.h"
#include "app.h"
#include "meter.h"
#include "nfcProtocol.h"
#include "NFC_i2c.h"
#include "test.h"

#define DATA_FLASH_TEST

#if defined(DATA_FLASH_TEST)
Date_t testDate;
static uint32 testMeterData = 0;
#endif

static void waitIdle()
{
	int idle = 0;
	for (int i = 0; i < 1000; i++) {
		if ((FCTL3 & BUSY) == 0) {
			idle = 1;
			break;
		}
	}
	if (idle == 0) {
		REBOOT_SYSTEM();
	}
}

static int isValidDate(uint8 year, uint8 mon, uint8 day)
{
	return (year > 20 && year < 100 && mon >= 1 && mon <= 12 && day >= 1 && day <= 31);
}

static int isValidMeterValue(uint8 *p)
{
	if ((*(p + 0) + *(p + 1) + *(p + 2) + *(p + 3)) == 0) {
		// all 0 --> invalid value
		return 0;
	}
#if 0
    char valueStr[0x10] = "";
    sprintf(valueStr, "%02X%02X%02X%02X", *(p+3), *(p+2), *(p+1), *(p+0));

    int valid = 1;
    for(int i = 0; i < strlen(valueStr); i++) {
        // string으로 변환했을 때 숫자가 아닌게 있으면 invalid
        if(valueStr[i] < '0' || valueStr[i] > '9') {
            valid = 0;
            break;
        }
    }
#else
	int valid = 1;
	for (int i = 0; i < 4; i++) {
		// 9보다 큰경우
		uint8 value = *(p + i);
		uint8 upper = (value >> 4) & 0x0f;
		uint8 lower = value & 0x0f;

		if (upper > 9 || lower > 9) {
			valid = 0;
			break;
		}
	}

#endif

	return valid;
}

static uint32 cal_checksum(uint8 *p, int len)
{
	uint32 checksum = 0;
	for (int i = 0; i < len; i++) {
		checksum += *(p + i);
	}
	return checksum;
}

static uint32 get_flashAddress(int sector)
{
	if (sector < FIRST_DATA_SECTOR || sector > MAP_SECTOR) {
		printf("invalid sector(%d)\n", sector);
		return 0;
	}

	return (uint32)(DATA_FLASH_TOP_ADDR + sector * SECTOR_SIZE);
}

static int readSector(int sector, uint8 *buf)
{
	memset(buf, 0, SECTOR_SIZE);
	uint32 *p32 = (uint32 *)buf;

	uint32 address = get_flashAddress(sector);
	if (address == 0) {
		return 0;
	}

	for (int i = 0; i < SECTOR_SIZE / 4; i++) {
		*(p32 + i) = __data20_read_long(address + i * 4);
	}

	return 1;
}

static void writeSector(int sector, uint8 *buf)
{
	uint32 address = get_flashAddress(sector);
	uint32 *p32 = (uint32 *)buf;

	if (address) {
		WDTCTL = WDTPW | WDTHOLD;

		halIntState_t intState;
		HAL_ENTER_CRITICAL_SECTION(intState);

		waitIdle();

		FCTL3 = FWKEY; // clear lock
		FCTL1 = FWKEY + ERASE; // Set Erase bit

		__data20_write_long(address, 0);

		// (Errata) Flash Read Error and Susceptibility for MSP430F54xxA
		asm(" bis.w #0,R3 "); // Flash contents: 0x03 0xd3
		asm(" bis.w #0,R3 "); // Flash contents: 0x03 0xd3
		asm(" bis.w #0,R3 "); // Flash contents: 0x03 0xd3

		FCTL3 = FWKEY; // clear lock
		FCTL1 = FWKEY + WRT; // Set Write bit

		for (int i = 0; i < SECTOR_SIZE / 4; i++) {
			__data20_write_long(address + i * 4, *(p32 + i));
			waitIdle();
		}

		FCTL1 = FWKEY;
		FCTL3 = FWKEY + LOCK;

		HAL_EXIT_CRITICAL_SECTION(intState);
		WDTCTL = WDT_ARST_16SEC;
	}
}

static void eraseSector(int sector)
{
	uint32 address = get_flashAddress(sector);
	if (address) {
		WDTCTL = WDTPW | WDTHOLD;

		halIntState_t intState;
		HAL_ENTER_CRITICAL_SECTION(intState);

		waitIdle();

		FCTL3 = FWKEY;
		FCTL1 = FWKEY + ERASE; // Set Erase bit

		__data20_write_char(address, 0);

		asm(" bis.w #0,R3 "); // Flash contents: 0x03 0xd3
		asm(" bis.w #0,R3 "); // Flash contents: 0x03 0xd3
		asm(" bis.w #0,R3 "); // Flash contents: 0x03 0xd3

		waitIdle();

		FCTL1 = FWKEY; // Set Erase bit
		FCTL3 = FWKEY + LOCK;

		HAL_EXIT_CRITICAL_SECTION(intState);
		WDTCTL = WDT_ARST_16SEC;
	}
}

static int read_mapSector(uint8 *buf)
{
	int result = 0;

	for (int retry = 0; retry < DATA_FLASH_RETRY_COUNT; retry++) {
		if (readSector(MAP_SECTOR, buf)) {
			map_sector_t *p = (map_sector_t *)buf;
			if (p->marker == ALL_FF && p->checksum == ALL_FF) {
				break;
			}

			if (p->marker == FLASH_MAP_MARKER) {
				uint32 checksum = cal_checksum(
					(uint8 *)p, (int)((uint8 *)&p->checksum - (uint8 *)p));
				if (checksum == p->checksum) {
					result = 1;
					break;
				} else {
					if (retry >= DATA_FLASH_RETRY_COUNT - 1) {
						printf("Map: checksum error [cal:%08lx read:%08lx]\n",
						       checksum, p->checksum);
					}
				}
			} else {
				if (retry >= DATA_FLASH_RETRY_COUNT - 1) {
					printf("Map: invalid marker: %08lx\n", p->marker);
				}
			}
		}
		MISC_delayMs(5);
	}
	return result;
}

static int read_dataSector(int sector, uint8 *buf)
{
	if (sector < FIRST_DATA_SECTOR || sector >= FIRST_DATA_SECTOR + NUM_DATA_SECTOR) {
		printf("# of sector should be within %d ~ %d\n", FIRST_DATA_SECTOR,
		       FIRST_DATA_SECTOR + NUM_DATA_SECTOR);
		return 0;
	}

	int result = 0;
	int emptySector = 0;

	int retry = 0;
	for (retry = 0; retry < DATA_FLASH_RETRY_COUNT; retry++) {
		if (readSector(sector, buf)) {
			data_sector_t *p = (data_sector_t *)buf;
			if (p->marker == ALL_FF && p->checksum == ALL_FF) {
				emptySector = 1;
				break;
			}

			if (p->marker == FLASH_DATA_MARKER) {
				int len = (int)((uint8 *)&p->checksum - (uint8 *)p);
				uint32 checksum = cal_checksum((uint8 *)p, len);
				if (checksum == p->checksum) {
					result = 1;
					break;
				} else {
					printf("Data sector %2d: checksum error [cal:%08lx read:%08lx] - len: %4d\n",
					       sector, checksum, p->checksum, len);
				}
			} else {
				printf("Data sector %2d: Marker invalid: %08lx\n", sector,
				       p->marker);
			}

			MISC_delayMs(5);
		}
	}

	if (result) {
		if (retry != 0) {
			printf(TP_ANSI_FG_RED);
			printf("*** sector %d read %d times ***\n", sector, retry + 1);
			printf(TP_ANSI_RESET);
		}
	} else {
		if (emptySector == 0) {
			printf("*** sector %d read Failed ***\n", sector);
		}
	}

	return result;
}

static int get_dataSavePos(uint8 year, uint8 mon, uint8 day)
{
	uint32 buf32[SECTOR_SIZE / 4];
	read_mapSector((uint8 *)buf32);

	map_sector_t *pMap = (map_sector_t *)buf32;

	int firstEmpty = -1;
	int matched = -1;
	int oldest = -1;

	uint32 minYMD = 0xFFFFFFFF;

	for (int unit = 0; unit < NUM_DATE_TO_STORE; unit++) {
		unit_map_t *pUnit = &pMap->unitMap[unit];

		if (isValidDate(pUnit->year, pUnit->mon, pUnit->day)) {
			if (pUnit->year == year && pUnit->mon == mon && pUnit->day == day) {
				matched = unit;
				break;
			}

			uint32 YMD = pUnit->year * 0x10000 + pUnit->mon * 0x100 + pUnit->day;
			if (YMD > 0 && minYMD > YMD) {
				minYMD = YMD;
				oldest = unit;
			}
		} else {
			if (firstEmpty < 0) {
				firstEmpty = unit;
			}
		}
	}

	if (matched >= 0) {
		return matched;
	}

	int savePos = 0;
	if (firstEmpty >= 0) {
		savePos = firstEmpty;
	} else if (oldest >= 0) {
		savePos = oldest;
	}

	unit_map_t *pUnitMap = &pMap->unitMap[savePos];
	memset((uint8 *)pUnitMap, 0, sizeof(unit_map_t));
	pUnitMap->year = year;
	pUnitMap->mon = mon;
	pUnitMap->day = day;
	pUnitMap->savePos = savePos;

	// update map sector
	pMap->checksum =
		cal_checksum((uint8 *)pMap, (int)((uint8 *)&pMap->checksum - (uint8 *)pMap));
	writeSector(MAP_SECTOR, (uint8 *)buf32);

	// init specified position of data sector
	int sector = savePos / UNIT_PER_DATA_SECTOR;
	int unit = savePos % UNIT_PER_DATA_SECTOR;

	if (read_dataSector(sector, (uint8 *)buf32) == 0) {
		memset((uint8 *)buf32, 0, SECTOR_SIZE);
	}

	data_sector_t *pData = (data_sector_t *)buf32;

	pData->marker = FLASH_DATA_MARKER;
	data_unit_t *pUnitData = &pData->dataUnit[unit];
	memset((uint8 *)pUnitData, 0, sizeof(data_unit_t));
	pUnitData->year = year;
	pUnitData->mon = mon;
	pUnitData->day = day;

	pData->checksum =
		cal_checksum((uint8 *)pData, (int)((uint8 *)&pData->checksum - (uint8 *)pData));
	writeSector(sector, (uint8 *)buf32);

	return savePos;
}

static int saveData(flash_save_data_t *newData)
{
	int savePos = get_dataSavePos(newData->year, newData->mon, newData->day);
	if (savePos < 0 || savePos >= (NUM_DATA_SECTOR * UNIT_PER_DATA_SECTOR)) {
		printf("dataFlash - get save position failed\n");
		return -1;
	}

	int sector = savePos / UNIT_PER_DATA_SECTOR;
	int unit = savePos % UNIT_PER_DATA_SECTOR;

	uint32 buf32[SECTOR_SIZE / 4];
	if (read_dataSector(sector, (uint8 *)buf32) == 0) {
		memset((uint8 *)buf32, 0, SECTOR_SIZE);
	}

	data_sector_t *pData = (data_sector_t *)buf32;

	pData->marker = FLASH_DATA_MARKER;

	data_unit_t *pUnit = &pData->dataUnit[unit];
	pUnit->year = newData->year;
	pUnit->mon = newData->mon;
	pUnit->day = newData->day;
	pUnit->caliber_dp = newData->caliber_dp;

	memcpy(pUnit->data[newData->hour], newData->meterValue, 4);

	if (isValidMeterValue(pUnit->data[newData->hour]) == 0) {
		printf("meter value invalid[%02X%02X%02X%02X]\n", pUnit->data[newData->hour][0],
		       pUnit->data[newData->hour][1], pUnit->data[newData->hour][2],
		       pUnit->data[newData->hour][3]);
		return -1;
	}

	pData->checksum =
		cal_checksum((uint8 *)pData, (int)((uint8 *)&pData->checksum - (uint8 *)pData));
	writeSector(sector, (uint8 *)buf32);

	return savePos;
}

static void verifySectors()
{
	uint32 buf32[SECTOR_SIZE / 4];

	int nInvalid = 0;
	// data sector - 비정상인 경우 초기화
	for (int sector = 0; sector < NUM_DATA_SECTOR; sector++) {
		if (read_dataSector(sector, (uint8 *)buf32) == 0) {
			data_sector_t *pData = (data_sector_t *)buf32;
			memset((uint8 *)buf32, 0, SECTOR_SIZE);
			pData->marker = FLASH_DATA_MARKER;
			pData->checksum = cal_checksum(
				(uint8 *)pData, (int)((uint8 *)&pData->checksum - (uint8 *)pData));
			writeSector(sector, (uint8 *)buf32);
			nInvalid++;
		}
		WDTCTL = WDT_ARST_16SEC;
	}
	if (nInvalid) {
		printf("%d data sector fixed\n", nInvalid);
	}

	// map sector - 비정상이면 초기화
	if (read_mapSector((uint8 *)buf32) == 0) {
		memset((uint8 *)buf32, 0, SECTOR_SIZE);
		map_sector_t *pMap = (map_sector_t *)buf32;
		pMap->marker = FLASH_MAP_MARKER;
		pMap->checksum = cal_checksum((uint8 *)pMap,
					      (int)((uint8 *)&pMap->checksum - (uint8 *)pMap));
		writeSector(MAP_SECTOR, (uint8 *)buf32);
		printf("map sector fixed\n");
	}
}

static void fix_mapSector()
{
	uint32 map32[SECTOR_SIZE / 4];
	uint32 data32[SECTOR_SIZE / 4];

	read_mapSector((uint8 *)map32);
	map_sector_t *pMap = (map_sector_t *)map32;

	data_unit_t lastUnit;
	memset(&lastUnit, 0, sizeof(data_unit_t));

	int mapChanged = 0;

	for (int nSector = 0; nSector < NUM_DATA_SECTOR; nSector++) {
		read_dataSector(nSector, (uint8 *)data32);
		data_sector_t *pData = (data_sector_t *)data32;

		for (int nUnit = 0; nUnit < UNIT_PER_DATA_SECTOR; nUnit++) {
			int savePos = nSector * UNIT_PER_DATA_SECTOR + nUnit;
			data_unit_t *pDataUnit = &pData->dataUnit[nUnit];
			unit_map_t *pMapUnit = &pMap->unitMap[savePos];

			if (isValidDate(pDataUnit->year, pDataUnit->mon, pDataUnit->day)) {
				// data sector의 해당 unit이 valid
				// map 정보가 잘못되었으면(날짜 정보가 다르거나 저장 위치가 다르면) map 재구성
				if (memcmp(&pDataUnit->year, &pMapUnit->year, 3) != 0 ||
				    pMapUnit->savePos != savePos) {
					pMapUnit->year = pDataUnit->year;
					pMapUnit->mon = pDataUnit->mon;
					pMapUnit->day = pDataUnit->day;
					pMapUnit->savePos = savePos;

					mapChanged = 1;
				}
#if defined(DATA_FLASH_TEST)
				if (memcmp(&lastUnit.year, &pDataUnit->year, 3) < 0) {
					memcpy(&lastUnit.year, &pDataUnit->year,
					       sizeof(data_unit_t));
				}
#endif
			} else {
				// data sector의 해당 unit이 invalid
				if (isValidDate(pMapUnit->year, pMapUnit->mon, pMapUnit->day)) {
					memset((uint8 *)pMapUnit, 0, sizeof(unit_map_t));
					mapChanged = 1;
				}
			}
		}
		WDTCTL = WDT_ARST_16SEC;
	}

	if (mapChanged) {
		pMap->checksum = cal_checksum((uint8 *)pMap,
					      (int)((uint8 *)&pMap->checksum - (uint8 *)pMap));
		writeSector(MAP_SECTOR, (uint8 *)map32);
	}

#if defined(DATA_FLASH_TEST)
	if (lastUnit.year) {
		testDate.year = lastUnit.year + 2000;
		testDate.mon = lastUnit.mon;
		testDate.day = lastUnit.day;
		testDate.hour = 0;
		testDate.min = testDate.sec = 0;

		for (int hour = 0; hour < 24; hour++) {
			uint32 value = 0;
			bcd2int(lastUnit.data[hour], &value, 4);
			if (value == 0) {
				break;
			}
			testDate.hour = hour;
			if (testMeterData < value) {
				testMeterData = value;
			}
		}
	}
#endif
}

static int readAndSort_map(map_sector_t *pMap, date_sort_t *pSort)
{
	fix_mapSector();
	read_mapSector((uint8 *)pMap);

	int nDay = 0;
	for (int nUnit = 0; nUnit < NUM_DATE_TO_STORE; nUnit++) {
		unit_map_t *pUnitMap = &pMap->unitMap[nUnit];
		if (isValidDate(pUnitMap->year, pUnitMap->mon, pUnitMap->day)) {
			date_sort_t *p = pSort + nDay;
			p->year = pUnitMap->year;
			p->mon = pUnitMap->mon;
			p->day = pUnitMap->day;
			p->savePos = pUnitMap->savePos;
			p->sortKey = pUnitMap->year * 400 + pUnitMap->mon * 31 + pUnitMap->day;
			nDay++;
		}
	}

	if (nDay >= 2) {
		for (int i = 0; i < nDay - 1; i++) {
			date_sort_t *top = pSort + i;
			for (int j = i + 1; j < nDay; j++) {
				date_sort_t *next = pSort + j;
				if (top->sortKey < next->sortKey) {
					date_sort_t temp;
					memcpy(&temp, next, sizeof(date_sort_t));
					memcpy(next, top, sizeof(date_sort_t));
					memcpy(top, &temp, sizeof(date_sort_t));
				}
			}
		}
	}

	return nDay;
}

static void hex2binary(uint8 hex, char *binary)
{
	*binary = '\0';

	for (int i = 7; i >= 0; i--) {
#if 1
		if (_IS_SET(hex, i)) {
			sprintf(binary, "%s1", binary);
		} else {
			sprintf(binary, "%s0", binary);
		}
#else
		if (_IS_SET(hex, i)) {
			binary[i] = 1;
		} else {
			binary[i] = 0;
		}
#endif
	}
}

static int make_dateList(int nDay, date_sort_t *pSort, day_data_map_t *pDayMap)
{
	int nMon = -1;
	int mon = -1;
	int year = -1;

	for (int i = 0; i < nDay; i++) {
		date_sort_t *p = pSort + i;
		if (year != p->year || mon != p->mon) {
			if (++nMon >= 5) {
				break;
			}
			year = p->year;
			mon = p->mon;
			pDayMap->unitDayDataMap[nMon].year = p->year;
			pDayMap->unitDayDataMap[nMon].mon = p->mon;
		}
		int nByte = (p->day - 1) / 8;
		int nBit = 7 - ((p->day - 1) % 8);
		pDayMap->unitDayDataMap[nMon].dayFlag[nByte] |= BM(nBit);
	}

	nMon += 1; // 시작이 -1이기 때문...

	for (int i = 0; i < nMon; i++) {
		unit_day_data_map_t *pUnitMap = &pDayMap->unitDayDataMap[i];
		if (isValidDate(pUnitMap->year, pUnitMap->mon, 1)) {
			printf("%04d-%02d ", pUnitMap->year + 2000, pUnitMap->mon);

			for (int j = 0; j < 4; j++) {
				char binary[0x10] = "";
				hex2binary(pUnitMap->dayFlag[j], binary);
				printf("%s ", binary);
			}
			printf("\n");
		}
	}

	return nMon;
}

static int read_dataUnit(data_unit_t *pWantedUnit, date_sort_t *pSort)
{
	int sector = pSort->savePos / UNIT_PER_DATA_SECTOR;
	int unit = pSort->savePos % UNIT_PER_DATA_SECTOR;

	uint32 buf32[SECTOR_SIZE / 4];
	read_dataSector(sector, (uint8 *)buf32);

	data_sector_t *pData = (data_sector_t *)buf32;
	data_unit_t *pUnit = &pData->dataUnit[unit];

	if (memcmp(&pSort->year, &pUnit->year, 3) == 0) {
		memcpy(pWantedUnit, pUnit, sizeof(data_unit_t));
		return 1;
	}

	return 0;
}

static void print_dataReport(NfcFlashDataReport_t *p)
{
	uint32 value = 0;
	bcd2int(p->meterData, &value, 4);

	printf("[%04d-%02d-%02d %02dH] %08ld [%02X] - %d/%d\n", p->year + 2000, p->mon, p->day,
	       p->hour, value, p->caliber_dp, p->nCurrent, p->nTotal);

	for (int i = 0; i < 12; i++) {
		uint16 diff = 0;
		memcpy(&diff, p->diff[i], 2);

		printf("  [%02d] ", i + p->hour);

		if (diff == 0xFFFF) {
			printf(TP_ANSI_FG_RED);
			printf("DATA EMPTY    ");
			printf(TP_ANSI_RESET);
		} else {
			value += diff;
			printf("%08ld (%2d) ", value, diff);
		}
		if ((i + 1) % 6 == 0) {
			printf("\n");
		}
	}
}

static void make_nfcDataReport(int part, int nSeq, int nTotal, NfcFlashDataReport_t *pMsg,
			       data_unit_t *pUnit)
{
	pMsg->mtype = FLASH_DATA_REPORT;
	pMsg->mversion = 0;
	pMsg->nTotal = nTotal;
	pMsg->nCurrent = nSeq;
	pMsg->caliber_dp = pUnit->caliber_dp;
	pMsg->year = pUnit->year;
	pMsg->mon = pUnit->mon;
	pMsg->day = pUnit->day;

	int firstHour = part * 12;
	pMsg->hour = firstHour;

	int refPos = -1;

	uint32 prevValue = 0;
	uint32 currValue = 0;
	uint16 diff = 0;

	for (int i = 0; i < 12; i++) {
		if (isValidMeterValue(pUnit->data[firstHour + i])) {
			if (refPos < 0) {
				refPos = i;
				memcpy(pMsg->meterData, pUnit->data[firstHour + i], 4);
				bcd2int(pUnit->data[firstHour + i], &prevValue, 4);
				diff = 0;
			} else {
				bcd2int(pUnit->data[firstHour + i], &currValue, 4);
				diff = currValue - prevValue;
				prevValue = currValue;
			}
		} else {
			diff = 0xFFFF;
		}
		memcpy(pMsg->diff[i], &diff, 2);
	}

	if (refPos >= 0) {
		pMsg->refPos = refPos;
	} else {
		pMsg->refPos = 0xFF;
	}
}

void dataFlash_eraseSector(char *str)
{
	if (strcmp(str, "all") == 0) {
		for (int sector = FIRST_DATA_SECTOR; sector < (NUM_DATA_SECTOR + NUM_MAP_SECTOR);
		     sector++) {
			eraseSector(sector);
		}
	} else {
		int sector = atoi(str);
		if (sector >= FIRST_DATA_SECTOR && sector <= (NUM_DATA_SECTOR + NUM_MAP_SECTOR)) {
			eraseSector(sector);
		} else {
			printf("invalid sector num(%s)\n", str);
		}
	}

	verifySectors();
	fix_mapSector();
}

void dataFlash_displayDataSector(int sector)
{
	uint32 buf32[SECTOR_SIZE / 4];

	if (read_dataSector(sector, (uint8 *)buf32)) {
		data_sector_t *pData = (data_sector_t *)buf32;
		for (int unit = 0; unit < UNIT_PER_DATA_SECTOR; unit++) {
			data_unit_t *pUnit = &pData->dataUnit[unit];
			if (isValidDate(pUnit->year, pUnit->mon, pUnit->day) == 0) {
				printf("[%2d:%d] empty\n", sector, unit);
				continue;
			}

			printf("[%2d:%d] %04d-%02d-%02d dp:%02X\n", sector, unit,
			       pUnit->year + 2000, pUnit->mon, pUnit->day, pUnit->caliber_dp);
			for (int hour = 0; hour < 24; hour++) {
				printf("   %2d) %02X%02X%02X%02X ", hour, pUnit->data[hour][0],
				       pUnit->data[hour][1], pUnit->data[hour][2],
				       pUnit->data[hour][3]);
				if ((hour + 1) % 6 == 0) {
					printf("\n");
				}
			}
		}
	}
}

void dataFlash_displayMapSector()
{
	uint32 buf32[SECTOR_SIZE / 4];
	map_sector_t *pMap = (map_sector_t *)buf32;

	date_sort_t SortedData[NUM_DATE_TO_STORE];
	memset(&SortedData, 0, sizeof(SortedData));

	int nDay = readAndSort_map(pMap, SortedData);
	if (nDay == 0) {
		return;
	}

	day_data_map_t dayDataMap;
	memset(&dayDataMap, 0, sizeof(dayDataMap));
	int nMonth = make_dateList(nDay, SortedData, &dayDataMap);

	for (int nSector = FIRST_DATA_SECTOR; nSector < NUM_DATA_SECTOR; nSector++) {
		printf("[sector %2d] ", nSector);
		for (int nUnit = 0; nUnit < UNIT_PER_DATA_SECTOR; nUnit++) {
			int pos = nSector * UNIT_PER_DATA_SECTOR + nUnit;
			unit_map_t *pUnit = &pMap->unitMap[pos];
			printf("(%d) ", nUnit);
			if (isValidDate(pUnit->year, pUnit->mon, pUnit->day)) {
				printf("%04d-%02d-%02d ", pUnit->year + 2000, pUnit->mon,
				       pUnit->day);
			} else {
				printf("           ");
			}
		}
		printf("\n");
		WDTCTL = WDT_ARST_16SEC;
	}
}

int dataFlash_getList(uint8 *pList)
{
	uint32 buf32[SECTOR_SIZE / 4];
	map_sector_t *pMap = (map_sector_t *)buf32;

	date_sort_t SortedData[NUM_DATE_TO_STORE];
	memset(&SortedData, 0, sizeof(SortedData));

	int nDay = readAndSort_map(pMap, SortedData);
	if (nDay == 0) {
		return 0;
	}

	day_data_map_t dayDataMap;
	memset(&dayDataMap, 0, sizeof(dayDataMap));
	int nMonth = make_dateList(nDay, SortedData, &dayDataMap);

	if (pList && nMonth) {
		memcpy(pList, &dayDataMap.unitDayDataMap[0], nMonth * sizeof(unit_day_data_map_t));
	}

	return nMonth;
}

int dataFlash_sendData(uint8 yearFrom, uint8 monFrom, uint8 dayFrom, uint8 yearTo, uint8 monTo,
		       uint8 dayTo)
{
	uint32 buf32[SECTOR_SIZE / 4];
	map_sector_t *pMap = (map_sector_t *)buf32;

	date_sort_t SortedData[NUM_DATE_TO_STORE];
	memset(&SortedData, 0, sizeof(SortedData));

	int nDay = readAndSort_map(pMap, SortedData);
	if (nDay == 0) {
		return 0;
	}

	ymd_t fromDate = { .year = yearFrom, .mon = monFrom, .day = dayFrom };
	ymd_t toDate = { .year = yearTo, .mon = monTo, .day = dayTo };

	int firstPos = -1;
	int lastPos = -1;

	int nTotalBlock = 0;
	for (int i = 0; i < NUM_DATE_TO_STORE; i++) {
		date_sort_t *p = &SortedData[i];
		if (memcmp(&p->year, &fromDate.year, 3) >= 0 &&
		    memcmp(&p->year, &toDate.year, 3) <= 0) {
			if (nTotalBlock == 0) {
				firstPos = i;
			}
			lastPos = i;

			p->txSeq = nTotalBlock;
			nTotalBlock += 2; // 하루를 두번에 나누어 보내므로 2를 증가시킴
		}
	}

	if (firstPos < 0 || lastPos < 0 || nTotalBlock == 0) {
		return 0;
	}

	byte msg[64];

	int resultOK = 1;
	for (int seq = 0; (seq < nTotalBlock / 2) && resultOK; seq++) {
		data_unit_t dataUnit;
		memset(&dataUnit, 0, sizeof(dataUnit));

		if (read_dataUnit(&dataUnit, &SortedData[seq + firstPos]) == 0) {
			continue;
		}

		for (int part = 0; part < 2; part++) {
			memset(&msg, 0, 64);
			NfcFlashDataReport_t *pMsg = (NfcFlashDataReport_t *)msg;

			int txSeqNo = seq * 2 + part + 1;
			make_nfcDataReport(part, txSeqNo, nTotalBlock, pMsg, &dataUnit);

			if (TEST_isTestMode()) {
				print_dataReport(pMsg);
			} else {
				NFCAPP_continueSend(msg, sizeof(NfcFlashDataReport_t));
				if (NFC_checkRead(100) == FALSE) {
					printf("Failed(%d/%d)\n", txSeqNo, nTotalBlock);
					resultOK = 0;
					break;
				}
			}
		}

		WDTCTL = WDT_ARST_16SEC;
	}

	return nTotalBlock;
}

void dataFlash_save(uint8 *pNewData, uint8 caliber_dp)
{
	MeterUnitData_t *pUnit = (MeterUnitData_t *)pNewData;

	flash_save_data_t data;
	memset(&data, 0, sizeof(flash_save_data_t));

	data.year = pUnit->year;
	data.mon = pUnit->mon;
	data.day = pUnit->day;
	data.hour = pUnit->hour;
	data.caliber_dp = caliber_dp;

	memcpy(data.meterValue, pUnit->meterData, 4);

	int sector = saveData(&data) / UNIT_PER_DATA_SECTOR;

	printf("[dataFlash_save] %04d-%02d-%02d %02dH - %02X%02X%02X%02X\n", data.year + 2000,
	       data.mon, data.day, data.hour, data.meterValue[0], data.meterValue[1],
	       data.meterValue[2], data.meterValue[3]);

	dataFlash_getList(NULL);
	dataFlash_displayDataSector(sector);
}

void dataFlash_readData(int yearFrom, int monFrom, int dayFrom, int nDay)
{
	Date_t from;
	memset(&from, 0, sizeof(Date_t));

	from.year = yearFrom;
	from.mon = monFrom;
	from.day = dayFrom;

	Date_t to;
	memset(&to, 0, sizeof(Date_t));
	memcpy(&to, &from, sizeof(Date_t));

	for (int i = 0; i < nDay - 1; i++) {
		RTC_incDateHour(&to, 24);
	}

	int nBlcok = dataFlash_sendData(from.year - 2000, from.mon, from.day, to.year - 2000,
					to.mon, to.day);
}

//yikim test data
int dataFlash_getCount()
{
	uint32 buf32[SECTOR_SIZE / 4];
	map_sector_t *pMap = (map_sector_t *)buf32;

	date_sort_t SortedData[NUM_DATE_TO_STORE];
	memset(&SortedData, 0, sizeof(SortedData));

	int nDay = readAndSort_map(pMap, SortedData);
	if (nDay == 0) {
		return 0;
	}

	ymd_t fromDate = { 21, 1, 1 };
	ymd_t toDate = { 22, 1, 1 };

	int firstPos = -1;
	int lastPos = -1;

	int nTotalBlock = 0;
	for (int i = 0; i < NUM_DATE_TO_STORE; i++) {
		date_sort_t *p = &SortedData[i];
		if (memcmp(&p->year, &fromDate.year, 3) >= 0 &&
		    memcmp(&p->year, &toDate.year, 3) <= 0) {
			if (nTotalBlock == 0) {
				firstPos = i;
			}
			lastPos = i;

			p->txSeq = nTotalBlock;
			nTotalBlock += 2; // 하루를 두번에 나누어 보내므로 2를 증가시킴
		}
	}

	if (firstPos < 0 || lastPos < 0 || nTotalBlock == 0) {
		return 0;
	}

	return nTotalBlock;
}

void dataFlash_init()
{
#if defined(DATA_FLASH_TEST)
	memset(&testDate, 0, sizeof(Date_t));
#endif

	verifySectors();
	fix_mapSector();
	dataFlash_getList(NULL);

#if defined(DATA_FLASH_TEST)
	if (testDate.year == 0) {
		testDate.year = 2021;
		testDate.mon = 9;
		testDate.day = 01;
		testDate.hour = 0;
		testDate.min = testDate.sec = 0;
		testMeterData = 100;
	}
#endif
}

void dataFlash_genData(int count, int skip)
{
#if defined(DATA_FLASH_TEST)
	for (int i = 0; i < count; i++) {
		int rvalue = rand() % 0x10;
		if (skip && rvalue == 0xF) {
			printf("\r[%04d-%02d-%02d %02dH] skipped                             \n",
			       testDate.year, testDate.mon, testDate.day, testDate.hour);
		} else {
			testMeterData += rvalue;

			flash_save_data_t newData;
			memset(&newData, 0, sizeof(flash_save_data_t));
			newData.year = testDate.year - 2000;
			newData.mon = testDate.mon;
			newData.day = testDate.day;
			newData.hour = testDate.hour;
			newData.caliber_dp = 0x13;
			int2bcd(&testMeterData, newData.meterValue, 4);

			int savePos = saveData(&newData);
			if (savePos >= 0) {
				printf("\r[%4d/%4d] %04d-%02d-%02d %02dH %08ld --> %d (%d: %d)",
				       i + 1, count, newData.year + 2000, newData.mon, newData.day,
				       newData.hour, testMeterData, savePos,
				       savePos / UNIT_PER_DATA_SECTOR,
				       savePos % UNIT_PER_DATA_SECTOR);
			}
		}
		RTC_incDateHour(&testDate, 1);
		WDTCTL = WDT_ARST_16SEC;
	}
	printf("\n");
#endif
}

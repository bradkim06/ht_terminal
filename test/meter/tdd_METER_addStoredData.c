#include "common_header.h"
#include "unity.h"
#include "tdd.h"
#include "meter.h"
#include "rtcAlarm.h"

typedef struct {
	int nData;
	int saveInterval;
} expect_t;

typedef struct {
	Config_t config;
	Date_t date;
	Date_t storedDate;
	MeterStoredData_t stored;
	MeterUnitData_t unit;
	expect_t expect;
} inputData_t;

inputData_t inputData[12] = {
	{
		// 1. 정상적인 검침데이터 저장
		.config = { 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 4, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 5, 1 },
	},
	{
		// 2. nData = 0
		.config = { 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 0, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 1, 1 },
	},
	{
		// 3. data0, interval0
		.config = { 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 0, .saveInterval = 0, 0 },
		.unit = { 0 },
		.expect = { 1, 1 },
	},
	{
		// 4. unexpected saveInterval init 1
		.config = { 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 24, .saveInterval = 0, 0 },
		.unit = { 0 },
		.expect = { 13, 2 },
	},
	{
		// 5. saveInterval 2, max
		.config = { 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 24, .saveInterval = 2, 0 },
		.unit = { 0 },
		.expect = { 13, 4 },
	},
	{
		// 6. saveInterval 4, max
		.config = { 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 17, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 24, .saveInterval = 4, 0 },
		.unit = { 0 },
		.expect = { 24, 4 },
	},
	{
		// 7. MAX 저장 초과
		.config = { 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 24, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 13, 2 },
	},
	{
		// 8. Save Time Interval 낮음
		.config = { 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 52, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.stored = { .nData = 1, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 1, 1 },
	},
	{
		// 9. Short Interval 저장
		.config = { .isShortInterval = 1, 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 52, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.stored = { .nData = 4, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 5, 1 },
	},
	{
		// 10. Short Interval Max
		.config = { .isShortInterval = 1, 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 52, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.stored = { .nData = 24, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 13, 2 },
	},
	{
		// 11. Short Interval 부족
		.config = { .isShortInterval = 1, 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.stored = { .nData = 1, .saveInterval = 24, 0 },
		.unit = { 0 },
		.expect = { 1, 24 },
	},
	{
		// 12. save Interval 24 초과 init
		.config = { .isShortInterval = 1, 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 14, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.stored = { .nData = 1, .saveInterval = 25, 0 },
		.unit = { 0 },
		.expect = { 2, 1 },
	},
};

Date_t storedDate;
MeterUnitData_t unit;
extern MeterStoredData_t StoredMeterData;
Config_t conf;

// CMock
#define MAX_NUM_STORED_DATA 24

int RTC_isTimeSync()
{
	return 1;
}

void meterDataSave(MeterUnitData_t *pUnit)
{
	for (int i = 0; i < 4; i++) {
		pUnit->meterData[i] = i;
	}

	pUnit->meterStatus = 0;
	pUnit->icon.lowBatt = 0;
	pUnit->icon.rArrow = 0;
	pUnit->icon.fArrow = 0; // always 0
	pUnit->icon.m3 = 1;
	pUnit->icon.notUsed = 0; // 표준 프로토콜에는 미사용 없음.
	pUnit->icon.leak = 0;

	uint8 serial[4];
	for (int i = 0; i < 4; i++) { // serial 4byte
		serial[i] = i;
	}

	uchar caliberDp = 0;
}

void setUp()
{
	memset(&StoredMeterData, 0, sizeof(MeterStoredData_t));
}

void tearDown()
{
}

static void testFunction(int idx)
{
	char msg[100] = "";
	memcpy(&conf, &inputData[idx].config, sizeof(Config_t));
	memcpy(&StoredMeterData, &inputData[idx].stored, sizeof(MeterStoredData_t));
	insertDateToData(&inputData[idx].storedDate, &StoredMeterData.unit[0], TRUE);
	METER_addStoredData(&inputData[idx].date, &inputData[idx].unit);

	sprintf(msg, "Test Case %d\n", idx);
	TEST_ASSERT_EQUAL_INT_MESSAGE(inputData[idx].expect.nData, StoredMeterData.nData, msg);
	TEST_ASSERT_EQUAL_INT_MESSAGE(inputData[idx].expect.saveInterval,
				      StoredMeterData.saveInterval, msg);
}

static void coverageDummy()
{
	Date_t date = { 0 };
	MeterUnitData_t unit = { 0 };
	// False Date
	insertDateToData(&date, &unit, TRUE);
	// no ignore sec
	insertDateToData(&inputData[0].date, &unit, FALSE);
	// no ignore sec
	copyDateFromData(&unit, &date, FALSE);
}

void test_METER_addStoredData()
{
	for (int i = 0; i < sizeof(inputData) / sizeof(inputData_t); i++) {
		testFunction(i);
	}

	coverageDummy();
}

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
	char testName[100];
	Config_t config;
	Date_t date;
	Date_t storedDate;
	MeterStoredData_t stored;
	MeterUnitData_t unit;
	expect_t expect;
} inputData_t;

static inputData_t inputData[19] = {
	{
		.testName = "정상적인 검침 데이터 저장",
		.config = { 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 4, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 5, 1 },
	},
	{
		.testName = "nData 0개, Save Data",
		.config = { 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 0, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 1, 1 },
	},
	{
		.testName = "interval 설정 0(Error), Init 1 Test",
		.config = { 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 0, .saveInterval = 0, 0 },
		.unit = { 0 },
		.expect = { 1, 1 },
	},
	{
		.testName = "interval 설정 25(Error), Init 1",
		.config = { 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 0, .saveInterval = 25, 0 },
		.unit = { 0 },
		.expect = { 1, 1 },
	},
	{
		.testName = "saveInterval 2->4, nData Max",
		.config = { 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 24, .saveInterval = 2, 0 },
		.unit = { 0 },
		.expect = { 13, 4 },
	},
	{
		.testName = "saveInterval 4 Not change, nData Max",
		.config = { 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 17, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 24, .saveInterval = 4, 0 },
		.unit = { 0 },
		.expect = { 24, 4 },
	},
	{
		.testName = "Diff Time is under saveInterval",
		.config = { 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 52, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.stored = { .nData = 1, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 1, 1 },
	},
	{
		.testName = "Short Config",
		.config = { .isShortInterval = 1, 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 52, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.stored = { .nData = 4, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 5, 1 },
	},
	{
		.testName = "Short Config, nData Max",
		.config = { .isShortInterval = 1, 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 52, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.stored = { .nData = 24, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 13, 2 },
	},
	{
		.testName = "Short Config, diff Time under Interval setting",
		.config = { .isShortInterval = 1, 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.stored = { .nData = 1, .saveInterval = 24, 0 },
		.unit = { 0 },
		.expect = { 1, 24 },
	},
	// Wrong Date Input Data, Coverage Test
	// 잘못된 Date가 들어오면 Diff가 충족되는것으로 처리
	{
		.testName = "Coverage Test, Under Year MIN",
		.config = { .isShortInterval = 0, 0 },
		.date = { .year = 0, .mon = 2, .day = 9, .hour = 14, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.stored = { .nData = 1, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 2, 1 },
	},
	{
		.testName = "Coverage Test, Over Year MAX",
		.config = { .isShortInterval = 0, 0 },
		.date = { .year = 2500, .mon = 2, .day = 9, .hour = 14, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.stored = { .nData = 1, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 2, 1 },
	},
	{
		.testName = "Coverage Test, Under Month MIN",
		.config = { .isShortInterval = 0, 0 },
		.date = { .year = 2022, .mon = 0, .day = 9, .hour = 14, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.stored = { .nData = 1, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 2, 1 },
	},
	{
		.testName = "Coverage Test, Over Month MAX",
		.config = { .isShortInterval = 0, 0 },
		.date = { .year = 2022, .mon = 13, .day = 9, .hour = 14, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.stored = { .nData = 1, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 2, 1 },
	},
	{
		.testName = "Coverage Test, Under Day MIN",
		.config = { .isShortInterval = 0, 0 },
		.date = { .year = 2022, .mon = 2, .day = 0, .hour = 14, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.stored = { .nData = 1, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 2, 1 },
	},
	{
		.testName = "Coverage Test, Over Day MAX",
		.config = { .isShortInterval = 0, 0 },
		.date = { .year = 2022, .mon = 2, .day = 33, .hour = 14, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.stored = { .nData = 1, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 2, 1 },
	},
	{
		.testName = "Coverage Test, Over Hour MAX",
		.config = { .isShortInterval = 0, 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 14, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 24, .min = 39, .sec = 0 },
		.stored = { .nData = 1, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 2, 1 },
	},
	{
		.testName = "Coverage Test, Over Minute MAX",
		.config = { .isShortInterval = 0, 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 14, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 14, .min = 60, .sec = 0 },
		.stored = { .nData = 1, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 2, 1 },
	},
	{
		.testName = "Coverage Test, Over Second MAX",
		.config = { .isShortInterval = 0, 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 14, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 14, .min = 39, .sec = 60 },
		.stored = { .nData = 1, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 2, 1 },
	},
};

Date_t storedDate;
MeterUnitData_t unit;
extern MeterStoredData_t StoredMeterData;
Config_t conf;

// CMock

int RTC_isTimeSync()
{
	return 1;
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
	// no ignore sec
	insertDateToData(&inputData[0].date, &unit, FALSE);
	// no ignore sec
	copyDateFromData(&unit, &date, FALSE);
}

void test_METER_addStoredData()
{
	char msg[100];
	for (int i = 0; i < sizeof(inputData) / sizeof(inputData_t); i++) {
		tddPrint("Test Case(%d) : %s\n", i, inputData[i].testName);
		Date_t *p = &inputData[i].date;
		sprintf(msg, "Date:%d-%d-%d,%d:%d:%d", p->year, p->mon, p->day, p->hour, p->min,
			p->sec);
		p = &inputData[i].storedDate;
		sprintf(msg, "%s Stored Date:%d-%d-%d,%d:%d:%d", msg, p->year, p->mon, p->day,
			p->hour, p->min, p->sec);
		tddPrint("%s\n", msg);

		testFunction(i);
	}

	coverageDummy();
}

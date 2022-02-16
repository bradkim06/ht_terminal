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
} storeInput_t;

static storeInput_t storeInput[20] = {
	{
		.testName = "정상적인 검침 데이터 저장",
		.config = { .dataSkipMode = 1 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 4, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 5, 1 },
	},
	{
		.testName = "Data Skip Mode OFF",
		.config = { 0 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 24, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 24, 1 },
	},
	{
		.testName = "nData 0개, Save Data",
		.config = { .dataSkipMode = 1 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 0, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 1, 1 },
	},
	{
		.testName = "interval 설정 0(Error), Init 1 Test",
		.config = { .dataSkipMode = 1 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 0, .saveInterval = 0, 0 },
		.unit = { 0 },
		.expect = { 1, 1 },
	},
	{
		.testName = "interval 설정 25(Error), Init 1",
		.config = { .dataSkipMode = 1 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 0, .saveInterval = 25, 0 },
		.unit = { 0 },
		.expect = { 1, 1 },
	},
	{
		.testName = "saveInterval 2->4, nData Max",
		.config = { .dataSkipMode = 1 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 24, .saveInterval = 2, 0 },
		.unit = { 0 },
		.expect = { 13, 4 },
	},
	{
		.testName = "saveInterval 4 Not change, nData Max",
		.config = { .dataSkipMode = 1 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 17, .min = 39, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 10, .min = 39, .sec = 0 },
		.stored = { .nData = 24, .saveInterval = 4, 0 },
		.unit = { 0 },
		.expect = { 24, 4 },
	},
	{
		.testName = "Diff Time is under saveInterval",
		.config = { .dataSkipMode = 1 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 52, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.stored = { .nData = 1, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 1, 1 },
	},
	{
		.testName = "Short Config",
		.config = { .isShortInterval = 1, .dataSkipMode = 1 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 52, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.stored = { .nData = 4, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 5, 1 },
	},
	{
		.testName = "Short Config, nData Max",
		.config = { .isShortInterval = 1, .dataSkipMode = 1 },
		.date = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 52, .sec = 0 },
		.storedDate = { .year = 2022, .mon = 2, .day = 9, .hour = 12, .min = 39, .sec = 0 },
		.stored = { .nData = 24, .saveInterval = 1, 0 },
		.unit = { 0 },
		.expect = { 13, 2 },
	},
	{
		.testName = "Short Config, diff Time under Interval setting",
		.config = { .isShortInterval = 1, .dataSkipMode = 1 },
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

static void test_addStoredData(int idx)
{
	char msg[100] = "";
	memcpy(&conf, &storeInput[idx].config, sizeof(Config_t));
	memcpy(&StoredMeterData, &storeInput[idx].stored, sizeof(MeterStoredData_t));
	insertDateToData(&storeInput[idx].storedDate, &StoredMeterData.unit[0], TRUE);
	METER_addStoredData(&storeInput[idx].date, &storeInput[idx].unit);

	sprintf(msg, "Test Case %d\n", idx);
	TEST_ASSERT_EQUAL_INT_MESSAGE(storeInput[idx].expect.nData, StoredMeterData.nData, msg);
	TEST_ASSERT_EQUAL_INT_MESSAGE(storeInput[idx].expect.saveInterval,
				      StoredMeterData.saveInterval, msg);
}

static void test_METER_clearStoredData(uint8 interval)
{
	memset(&conf, 0, sizeof(Config_t));
	memset(&StoredMeterData, 0xff, sizeof(MeterStoredData_t));
	conf.meterInterval = interval;
	uint8 expectInterval = interval;
	if (interval < 1 || interval > 24) {
		expectInterval = 1;
	}

	METER_clearStoredData();

	tddPrint("expect nData(0), interval(%d), allMeterUnit(0)\n", expectInterval);

	MeterStoredData_t *p = &StoredMeterData;
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, p->nData, "nData must be 0");
	TEST_ASSERT_EQUAL_UINT8_MESSAGE(expectInterval, p->saveInterval, "Stored interval test");
	TEST_ASSERT_EQUAL_UINT8_MESSAGE(expectInterval, conf.meterInterval, "Stored interval test");

	uint8 *pUnit = (uint8 *)&p->unit;
	for (int i = 0; i < sizeof(p->unit); i++) {
		TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, *(pUnit + i), "meter unit data must be 0");
	}
}

static void test_METER_clearIntervalData(int reportInterval, int nData)
{
	conf.reportInterval = reportInterval;
	memset(&StoredMeterData, 0xff, sizeof(MeterStoredData_t));
	StoredMeterData.nData = nData;

	uint8 expectInterval = reportInterval;
	if (reportInterval < 1 || reportInterval > 24) {
		expectInterval = 6;
	}

	METER_clearIntervalData();
	tddPrint("input interval(%d) expect(%d)\n", reportInterval, expectInterval);
	TEST_ASSERT_EQUAL_UINT8_MESSAGE(expectInterval, conf.reportInterval,
					"reportInterval Check");
	tddPrint("input nData(%d) expect(%d)\n", nData, StoredMeterData.nData);
	TEST_ASSERT_EQUAL_INT_MESSAGE(NUM_NBIOT_STORED_DATA - expectInterval, StoredMeterData.nData,
				      "nData - reportInterval");

	for (int i = NUM_NBIOT_STORED_DATA - 1; i >= NUM_NBIOT_STORED_DATA - expectInterval; i--) {
		uint8 *pData = (uint8 *)&StoredMeterData.unit[i];
		MeterUnitData_t *p = &StoredMeterData.unit[i];

		for (int k = 0; k < sizeof(MeterUnitData_t); k++) {
			TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, *(pData + k), "unit data must be 0");
		}
	}
}

static void coverageDummy()
{
	Date_t date = { 0 };
	MeterUnitData_t unit = { 0 };
	// no ignore sec
	insertDateToData(&storeInput[0].date, &unit, FALSE);
	// no ignore sec
	copyDateFromData(&unit, &date, FALSE);

	StoredMeterData.nData = 1;
	METER_clearIntervalData();
}

void test_METER()
{
	char msg[100];
	TEST_MESSAGE("METER_addStoredData Test");
	for (int i = 0; i < sizeof(storeInput) / sizeof(storeInput_t); i++) {
		tddPrint("Test Case(%d) : %s\n", i, storeInput[i].testName);
		Date_t *p = &storeInput[i].date;
		sprintf(msg, "Date:%d-%d-%d,%d:%d:%d", p->year, p->mon, p->day, p->hour, p->min,
			p->sec);
		p = &storeInput[i].storedDate;
		sprintf(msg, "%s Stored Date:%d-%d-%d,%d:%d:%d", msg, p->year, p->mon, p->day,
			p->hour, p->min, p->sec);
		tddPrint("%s\n", msg);

		test_addStoredData(i);
	}

	TEST_MESSAGE("METER_clearStoredData Test");
	test_METER_clearStoredData(2);
	test_METER_clearStoredData(0);
	test_METER_clearStoredData(25);

	TEST_MESSAGE("METER_clearIntervalData");
	test_METER_clearIntervalData(6, NUM_NBIOT_STORED_DATA);
	test_METER_clearIntervalData(1, NUM_NBIOT_STORED_DATA);
	test_METER_clearIntervalData(0, NUM_NBIOT_STORED_DATA);
	test_METER_clearIntervalData(25, 25);

	TEST_MESSAGE("coverage Dummy Test");
	coverageDummy();
}

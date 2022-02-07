#include <string.h>

#include "unity.h"
#include "common_header.h"
#include "flashDriver.h"
#include "tdd.h"
#include "app.h"

typedef struct {
	int serialNum;
	uint8 reportRange;
	uint8 reportInterval;
	uint8 resultHour;
	uint8 resultMin;
} inputData_t;

inputData_t inputData[4] = { { 7034, 6, 6, 2, 39 },
			     { 2634, 6, 6, 4, 39 },
			     { 6234, 3, 6, 1, 39 },
			     { 4891, 1, 6, 0, 46 } };
Config_t config;

void setUp()
{
	memset(&config, 0, sizeof(Config_t));
}

void tearDown()
{
}

// static void report(Config_t *conf)
// {
// 	tddPrint("reportMin : %d intervalBase : %d reportInterval : %d\n", conf->reportMin,
// 		 conf->intervalBaseTime, conf->reportInterval);
// 	for (int h = 0; h < 24; h++) {
// 		if (APP_checkTimeInterval(h, conf->intervalBaseTime, conf->reportInterval)) {
// 			tddPrint("%d ", h);
// 		}
// 	}
// 	tddPrint("\n");
// }
//
static void testFunction(inputData_t *data)
{
	char msg[100] = "";
	config.reportRange = data->reportRange;
	config.reportInterval = data->reportInterval;
	sprintf(msg, "s/n : %d reportRange : %d Interval : %d expectHour : %d expectMin : %d",
		data->serialNum, config.reportRange, config.reportInterval, data->resultHour,
		data->resultMin);

	distributingReportTime(data->serialNum, &config);
	TEST_ASSERT_EQUAL_INT_MESSAGE(data->resultHour, config.intervalBaseTime, msg);
	TEST_ASSERT_EQUAL_INT_MESSAGE(data->resultMin, config.reportMin, msg);
}

void test_distributeReportTime()
{
	for (int i = 0; i < sizeof(inputData) / sizeof(inputData_t); i++) {
		testFunction(&inputData[i]);
	}
}

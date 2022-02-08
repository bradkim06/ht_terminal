/**
 * @file tdd_distributeReportTime.c
 * @brief 보고시간 분산 기능 Test
 * 일련번호 끝 4자리, 보고주기(reportRange)로 아래 식으로 분산됨
 * serialBase = 0537, baseTime = (537%300)/50 = 4, 
 * reportMin = ((537%300)%50)+5 = 42 reportSec = ((537/300)%4) * 15 = 0
 * @author Kim Junsu
 * @version 1.00
 * @date 2022-02-08
 */

#include <string.h>
#include <stdio.h>

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

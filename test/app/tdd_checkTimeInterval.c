/**
 * @file tdd_checkTimeInterval.c
 * @brief 주기보고 분산 Function Test
 *
 * @author Kim Junsu
 * @version 1.00
 * @date 2022-01-28
 */

#include "tdd.h"
#include "app.h"
#include "unity.h"
#include <stdio.h>

static int result = 0;
static int intervalArr[] = { 1, 2, 3, 4, 6, 12, 24 };

/**
 * @brief input data {current hour, base time, interval, expect}
 */
static int inputData[24][4] = {
	{ 0, 0, 6, 1 },	 { 1, 0, 6, 0 },  { 2, 0, 6, 0 },  { 3, 0, 6, 0 },  { 4, 0, 6, 0 },
	{ 5, 0, 6, 0 },	 { 6, 0, 6, 1 },  { 7, 0, 6, 0 },  { 8, 0, 6, 0 },  { 9, 0, 6, 0 },
	{ 10, 0, 6, 0 }, { 11, 0, 6, 0 }, { 12, 0, 6, 1 }, { 13, 0, 6, 0 }, { 14, 0, 6, 0 },
	{ 15, 0, 6, 0 }, { 16, 0, 6, 0 }, { 17, 0, 6, 0 }, { 18, 0, 6, 1 }, { 19, 0, 6, 0 },
	{ 20, 0, 6, 0 }, { 21, 0, 6, 0 }, { 22, 0, 6, 0 }, { 23, 0, 6, 0 }
};

static int expectFunc(int h, int bt, int interval)
{
	int result = 0;

	int hour = h + 24 - bt;
	if ((hour % interval) == 0) {
		result = 1; // report required.
	}

	return result;
}

void setup()
{
}

void tearDown()
{
}

void test_reportInterval()
{
	char msg[1000] = "";

	// expect function test
	for (int base = 0; base < 24; base++) {
		for (int h = 0; h < 24; h++) {
			for (int i = 0; i < sizeof(intervalArr) / sizeof(int); i++) {
				sprintf(msg, "inputData -> h : %d base : %d interval : %d\n", h,
					base, intervalArr[i]);
				int expect = expectFunc(h, base, intervalArr[i]);
				int result = APP_checkTimeInterval(h, base, intervalArr[i]);
				TEST_ASSERT_EQUAL_INT_MESSAGE(expect, result, msg);
			}
		}
	}

	int size = sizeof(inputData) / sizeof(inputData[0]);
	// input data list test
	for (int i = 0; i < size; i++) {
#define CURRENT_HOUR 0
#define BASE_TIME 1
#define INTERVAL 2
#define EXPECT 3
		int h = inputData[i][CURRENT_HOUR];
		int base = inputData[i][BASE_TIME];
		int interval = inputData[i][INTERVAL];
		int expect = inputData[i][EXPECT];

		sprintf(msg, "inputData -> h : %d base : %d interval : %d\n", h, base, interval);
		int result = APP_checkTimeInterval(h, base, interval);
		TEST_ASSERT_EQUAL_INT_MESSAGE(expect, result, msg);
	}
}

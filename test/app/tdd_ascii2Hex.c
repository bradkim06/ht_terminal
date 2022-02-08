/**
 * @file tdd_ascii2Hex.c
 * @brief ascii char to Hex Test
 * @author Kim Junsu
 * @version 1.00
 * @date 2022-02-08
 */

#include "unity.h"
#include "flashDriver.h"
#include "tdd.h"

typedef struct {
	char inputChar;
	uint8 expect;
} inputData_t;

inputData_t inputData[12] = { { '1', 1 }, { '2', 2 },  { '3', 3 },  { '4', 4 },
			      { '5', 5 }, { '6', 6 },  { '7', 7 },  { '8', 8 },
			      { '9', 9 }, { 'a', 10 }, { 'A', 10 }, { ' ', 0 } };

void setUp()
{
}

void tearDown()
{
}

static void testFunction(inputData_t *data)
{
	char msg[100] = "";
	uint8 result = ascii2Hex(data->inputChar);
	TEST_ASSERT_EQUAL_INT_MESSAGE(data->expect, result, msg);
}

void test_ascii2Hex()
{
	for (int i = 0; i < sizeof(inputData) / sizeof(inputData_t); i++) {
		testFunction(&inputData[i]);
	}
}

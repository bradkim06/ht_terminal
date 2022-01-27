/**
 * @file tdd_parseQLWULDATAEX.c
 * @brief QLWULDATAEX는 LwM2M Data Uplink결과를 Comfirmable한다.
 * 모뎀은 Uplink 결과를 "+QLWULDATASTATUS:<status>"로 알려준다.
 * <status> Integer type. Status of CON data sending.
    0 Have not been sent
    1 Sent, waiting response of IoT platform
    2 Sent failed
    3 Timeout
    4 Success
    5 Got reset message
 * @author Kim Junsu
 * @version 1.00
 * @date 2022-01-28
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nbiotModem.h"
#include "nbiotResponse.h"
#include "unity.h"
#include "tdd.h"

static unsigned char result;
static char *string;
static ModemContext_t modem;

void setUp()
{
	memset(&modem, 0, sizeof(ModemContext_t));
	string = malloc(sizeof(char) * 100);
}
void tearDown()
{
	free(string);
}

static void testFunction(unsigned char recvStatus, unsigned char success, char *testName, char *str)
{
	tddPrint("\n============ %s Test Case ============\n", testName);
	tddPrint("input string : %s\n", str);
	modem.waitDl = 0;
	result = parseQLWULDATAEX(string, &modem);
	TEST_ASSERT_EQUAL_INT_MESSAGE(recvStatus, result, "QLWULDATAEX parsing fail");
	TEST_ASSERT_EQUAL_INT_MESSAGE(success, modem.waitDl, "Modem waitDl Flag expect set");
}

void test_parseQLWULDATAEX()
{
	sprintf(string, "+QLWULDATASTATUS:4");
	testFunction(1, 1, "Success(4)", string);

	sprintf(string, "+QLWULDATASTATUS:3");
	testFunction(1, 0, "Wrong Status", string);

	sprintf(string, "+TESTTESTTEST");
	testFunction(0, 0, "Wrong String", string);
}

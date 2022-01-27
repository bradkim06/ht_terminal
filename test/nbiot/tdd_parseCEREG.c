/**
 * @file tdd_parseCEREG.c
 * @brief AT+CEREG? Command의 Response String Parsing Test
 * Response : +CEREG:<n>,<stat>[,[<tac>],[<ci>],[<AcT>],[<rac>][,[<cause_type>],[<reject_cause>][,[<Active-Time>],[<Periodic-TAU>]]]]
 * <stat> Integer type. The EPS registration status.
     0 Not registered, UE is not currently searching an operator to register to
     1 Registered, home network
     2 Not registered, but UE is currently trying to attach or searching an operator to register to
     3 Registration denied
     4 Unknown (e.g. out of E-UTRAN coverage)
     5 Registered, roaming
 * @caution Attach Status가 바뀌면 모뎀이 자동으로 알려주는 unsolicited result String은 <n>이 없다.
 * unsolicited result String은 해당 함수에서 Parsing하지 않는다.
 * @author Kim Junsu
 * @version 
 * @date 2022-01-28
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nbiotModem.h"
#include "nbiotResponse.h"
#include "unity.h"
#include "tdd.h"

#define PATTERN_CEREG_PREFIX "+CEREG:5"

static unsigned char result;
static char *p = NULL;
static ModemContext_t modem;
static char *string;

void setUp()
{
	memset(&modem, 0, sizeof(ModemContext_t));
	string = malloc(sizeof(char) * 500);
}
void tearDown()
{
	free(string);
}

/**
 * @brief Parse CEREG Test전 ModemContext 초기화
 *
 * @param modemPtr 모뎀 상태 구조체
 */
static void setModemAttachStatus(ModemContext_t *modemPtr)
{
	modemPtr->status.cellreg = 0;
	modemPtr->lwm2m.regFinish = 1;
	modemPtr->lwm2m.obsObj10250 = 1;
	modemPtr->lwm2m.obsObj16241 = 1;
}

/**
 * @brief Attach Status에 따른 lwm2m Status Test
 *
 * @param result Attach Status Result
 * @param modemPtr 모뎀 상태 구조체
 */
static void checkLWM2MStatus(unsigned char result, ModemContext_t *modemPtr)
{
	if (result != 1) {
		TEST_ASSERT_FALSE_MESSAGE(modemPtr->lwm2m.regFinish, "regFinish flag clear fail");
		TEST_ASSERT_FALSE_MESSAGE(modemPtr->lwm2m.obsObj10250, "obj10250 flag clear fail");
		TEST_ASSERT_FALSE_MESSAGE(modemPtr->lwm2m.obsObj16241, "obj16241 flag clear fail");
	}
}

/**
 * @brief Attach Test Function
 *
 * @param expect Attach Status return
 * @param testName Test Case NAme
 * @param str Test Input String
 */
static void testFunction(unsigned char expect, char *testName, char *str)
{
	tddPrint("\n============ %s Test Case ============\n", testName);
	tddPrint("input string : %s\n", str);
	setModemAttachStatus(&modem);
	char *p = NULL;
	if ((p = strstr(string, PATTERN_CEREG_PREFIX)) == NULL) {
		tddPrint("+CEREG:5 not find\n");
		return;
	}
	result = parse_cereg(p, &modem);
	TEST_ASSERT_EQUAL_INT_MESSAGE(expect, result, "attach status fail");
	checkLWM2MStatus(result, &modem);
}

void test_parse_cereg()
{
	// 정상적인 Attach Case
	sprintf(string, "..+CEREG:5,1,213D,0318886E,9,,,,....OK..");
	testFunction(1, "Attach Success", string);

	sprintf(string, "..+CEREG:5,0,213D,0318886E,9,,,,....OK..");
	testFunction(0, "Attach Fail(Not Register, UE isn't trying attach)", string);

	sprintf(string, "..+CEREG:5,2,213D,0318886E,9,,,,....OK..");
	testFunction(2, "Attach Fail(Not Register, UE trying attach)", string);

	sprintf(string, "..+CEREG:5,3,213D,0318886E,9,,,,....OK..");
	testFunction(3, "Attach Fail(Registration denied)", string);

	sprintf(string, "..+CEREG:5,4,213D,0318886E,9,,,,....OK..");
	testFunction(4, "Attach Fail(out of E-UTRAN coverage)", string);

	/* +CEREG:1은 모뎀이 Attach Status가 변경되면 자동으로 알리는 상황으로 이
     * 함수에서 처리해도 상관없으나 코드 일관성을 위해 처리하지 않는다. */
	sprintf(string, "..+CEREG:1,213D,0318886E,9,,,,..");
	testFunction(0, "Modem Attach 상태 변경 자동 알림", string);

	sprintf(string, "..+CCLK:21/06/10,17:22:58+36....OK....+CEREG:5,2,213D,0318886E,9,,,,..");
	testFunction(2, "다른 Response 섞이고, Attach 실패", string);

	sprintf(string, "..+CEREG:1,213D,0318886E,9,,,,....NUESTATS:RADIO,Signal "
			"power:-737....NUESTATS:RADIO,Total power:-668....NUESTATS:RADIO,TX "
			"power:-40....NUESTATS:RADIO,TX time:418....NUESTATS:RADIO,RX "
			"time:3552....NUESTATS:RADIO,Cell "
			"ID:51939438....NUESTATS:RADIO,ECL:0....NUESTATS:RADIO,SNR:204......+"
			"CEREG:5,1,213D,0318886E,9,,,,....OK..NUESTATS:RADIO,EARFCN:2590...."
			"NUESTATS:RADIO,PCI:133....NUESTATS:RADIO,RSRQ:-108....NUESTATS:RADIO,"
			"OPERATOR MODE:2....NUESTATS:RADIO,CURRENT BAND:5....OK..");
	testFunction(1, "다른 Response 섞이고, Attach 성공", string);
}

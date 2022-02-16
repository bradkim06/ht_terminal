#include "unity.h"
#include "tdd.h"
#include "nfcMock.h"
#include "common_header.h"
#include "app.h"
#include "nfcProtocol.h"

uint8 AppTaskId = 0;
Config_t conf;

void setUp(void)
{
}

void tearDown(void)
{
}

nfcExpect_t result = { 0 };
static bdReqInput_t bdReqInput[3] = { { .testName = "Data Skip Mode ON",
					.data = { .mtype = BD_CONTROL_REQ,
						  .mversion = NFC_PROTOCOL_VER_5,
						  .dataSkipMode = DATASKIP_MODE_ON },
					.expect = { .osalEvent = APP_EVENT_CHANGE_CONFIG,
						    .conf = { .dataSkipMode = 1 },
						    .msg = { .mtype = BD_CONTROL_ACK,
							     .mversion = NFC_PROTOCOL_VER_5,
							     .reset = 0,
							     .sleepMode = SLEEP_MODE_NORMAL,
							     .reportMode = REPORT_MODE_SERVER,
							     .periodMode = PERIOD_MODE_NO_USE,
							     .debugMode = DEBUG_MODE_NO_USE,
							     .dataSkipMode = DATASKIP_MODE_ON },
						    .sendLen = 0 } },

				      { .testName = "Data Skip Mode No Change",
					.data = { .mtype = BD_CONTROL_REQ,
						  .mversion = NFC_PROTOCOL_VER_5,
						  .dataSkipMode = DATASKIP_MODE_NO_CHANGE },
					.expect = { .osalEvent = 0,
						    .conf = { .dataSkipMode = 0 },
						    .msg = { .mtype = BD_CONTROL_ACK,
							     .mversion = NFC_PROTOCOL_VER_5,
							     .reset = 0,
							     .sleepMode = SLEEP_MODE_NORMAL,
							     .reportMode = REPORT_MODE_SERVER,
							     .periodMode = PERIOD_MODE_NO_USE,
							     .debugMode = DEBUG_MODE_NO_USE,
							     .dataSkipMode = DATASKIP_MODE_OFF },
						    .sendLen = 0 } },

				      { .testName = "Data Skip Mode OFF",
					.data = { .mtype = BD_CONTROL_REQ,
						  .mversion = NFC_PROTOCOL_VER_5,
						  .dataSkipMode = DATASKIP_MODE_OFF },
					.expect = { .osalEvent = APP_EVENT_CHANGE_CONFIG,
						    .conf = { .dataSkipMode = 0 },
						    .msg = { .mtype = BD_CONTROL_ACK,
							     .mversion = NFC_PROTOCOL_VER_5,
							     .reset = 0,
							     .sleepMode = SLEEP_MODE_NORMAL,
							     .reportMode = REPORT_MODE_SERVER,
							     .periodMode = PERIOD_MODE_NO_USE,
							     .debugMode = DEBUG_MODE_NO_USE,
							     .dataSkipMode = DATASKIP_MODE_OFF },
						    .sendLen = 0 } } };

static void test_dataSkipMode(int i)
{
	// initialize
	memset(&conf, 0, sizeof(Config_t));
	memset(&result, 0, sizeof(result));
	NfcBdCtrlReqV5_t *p = &bdReqInput[i].data;
	printf("input mtype(%d), mversion(%d), reset(%d), sleep(%d), reportMode(%d), period(%d), debug(%d), dataSkip(%d)\n",
	       p->mtype, p->mversion, p->reset, p->sleepMode, p->reportMode, p->periodMode,
	       p->debugMode, p->dataSkipMode);

	// run function
	recvBdControlReq((byte *)&bdReqInput[i].data, sizeof(NfcBdCtrlReqV5_t));
	TEST_ASSERT_EQUAL_HEX(bdReqInput[i].expect.osalEvent, result.osalEvent);
	printf("Config Result - sleep(%d) riMode(%d) riChgCount(%d) riCtrlValue(%d) period(%d) debug(%d) dataSkip(%d)\n",
	       conf.sleepMode, conf.riCtrlMode, conf.riCtrlChgCount, conf.riCtrlValue,
	       conf.periodMode, conf.debugPrint, conf.dataSkipMode);

	// result test BD_CONTROL_REQ
	uint8 *pExpect = (uint8 *)&bdReqInput[i].expect.conf;
	uint8 *pResult = (uint8 *)&conf;
	for (int i = 0; i < sizeof(Config_t); i++) {
		TEST_ASSERT_EQUAL_UINT8(*(pExpect + i), *(pResult + i));
	}

	// result test BD_CONTROL_ACK
	NfcBdCtrlAckV5_t *ptr = (NfcBdCtrlAckV5_t *)&result.msg;
	if (ptr->mversion == NFC_PROTOCOL_VER_5) {
		printf("send NFC Result, mtype(%d) mversion(%d) reset(%d) sleep(%d) report(%d) period(%d) debug(%d) dataSkip(%d)\n",
		       ptr->mtype, ptr->mversion, ptr->reset, ptr->sleepMode, ptr->reportMode,
		       ptr->periodMode, ptr->debugMode, ptr->dataSkipMode);
	}

	printf("Hex Msg - ");
	pExpect = (uint8 *)&bdReqInput[i].expect.msg;
	pResult = (uint8 *)&result.msg;
	for (int i = 0; i < sizeof(NfcBdCtrlAckV5_t); i++) {
		printf("%02X ", *(pExpect + i));
		TEST_ASSERT_EQUAL_HEX8(*(pExpect + i), *(pResult + i));
	}
	printf("\n");
}

void test_nfcProtocol(void)
{
	// recvBdControlReq() Test
	for (int i = 0; i < sizeof(bdReqInput) / sizeof(bdReqInput_t); i++) {
		printf("================ Test Case (%d) - %s ================\n", i,
		       bdReqInput[i].testName);
		test_dataSkipMode(i);
	}
}

#include "common_header.h"
#include "battery.h"

// CMock
uint8 BATT_getVoltage()
{
	// 3.6V
	uint8 battLevel = 36;
	return battLevel;
}

#include "unity.h"
#include "flashDriver.h"
#include "message.h"
#include "modem.h"
#include "tdd.h"
#include "nbiotResponse.h"
#include "nbiotModem.h"

#define EXPECT_MSG_VER 4

typedef struct {
	Modem_t modem;
	char modemFwVer[30];
	uchar deviceFwVer;
	uchar initialReport;
	NbiotQaReportToLg_t expect;
} inputData_t;

inputData_t inputData[2] = {
	{ .modem = { .ctnStr = "01222991234",
		     .modemQuality = { .cgi = 0x3151582,
				       .rsrp = -950,
				       .snr = 120,
				       .txPower = -120 } },
	  .modemFwVer = "..BC95GJBR02A02_LGU....OK..",
	  .deviceFwVer = TERM_MODEL_HAT_124W,
	  .initialReport = 0,
	  .expect = { .msgVer = EXPECT_MSG_VER,
		      .ctn = { 0x00, 0x12, 0x22, 0x99, 0x12, 0x34 },
		      .batt = { 0x1, 0x3, 0x60 },
		      .cgi = { 0x03, 0x15, 0x15, 0x82 },
		      .rsrp = { 0x00, 0x95 },
		      .sinr = { 0x00, 0x12 },
		      .model = { 8, 'H', 'A', 'T', '-', '1', '2', '4', 'W' },
		      .fwVer = { 19,  'U', '3', '2', '5', '/', 'B', 'C', '9', '5',
				 'G', 'J', 'B', 'R', '0', '2', 'A', '0', '2', '_' },
		      .txPower = { 0x01, 0x12 },
		      .ueInfo = 0x42,
		      .reserved = { 0xf1, 0xf1, 0xf1 } } },

	{ .modem = { .ctnStr = "01234567890",
		     .modemQuality = { .cgi = 0x1234567, .rsrp = -1120, .snr = -50, .txPower = 50 } },
	  .modemFwVer = "..BC95GJBR....OK..",
	  .deviceFwVer = TERM_MODEL_HAT_435W,
	  .initialReport = 1,
	  .expect = { .msgVer = EXPECT_MSG_VER,
		      .ctn = { 0x00, 0x12, 0x34, 0x56, 0x78, 0x90 },
		      .batt = { 0x1, 0x3, 0x60 },
		      .cgi = { 0x01, 0x23, 0x45, 0x67 },
		      .rsrp = { 0x01, 0x12 },
		      .sinr = { 0x01, 0x05 },
		      .model = { 8, 'H', 'A', 'T', '-', '4', '3', '5', 'W' },
		      .fwVer = { 19, 'U', '3', '2', '5', '/', 'B', 'C', '9', '5', 'G', 'J', 'B',
				 'R' },
		      .txPower = { 0x00, 0x05 },
		      .ueInfo = 0x43,
		      .reserved = { 0xf1, 0xf1, 0xf1 } } }
};

typedef struct {
	uint8 input;
	uint8 expect;
} coverageTest_t;

coverageTest_t coverage[6] = { { .input = TERM_MODEL_UNKNOWN, .expect = 0 },
			       { .input = TERM_MODEL_HAT_114W, .expect = 1 },
			       { .input = TERM_MODEL_HAT_124W, .expect = 2 },
			       { .input = TERM_MODEL_HTM_115W, .expect = 3 },
			       { .input = TERM_MODEL_HAT_314W, .expect = 4 },
			       { .input = TERM_MODEL_HAT_435W, .expect = 5 } };

uchar raw_data[LEN_MAX_NBIOT_DATA];
Config_t conf;
Modem_t modem;
ModemContext_t modemCtx;

void setUp(void)
{
}

void tearDown(void)
{
}

static void testFunction_hex(uchar *p, char *name, uchar *input, int size)
{
	char msg[100] = "";

	sprintf(msg, "%s 0x", name);
	for (int i = 0; i < size; i++) {
		TEST_ASSERT_EQUAL_HEX8_MESSAGE(*(input + i), *(p + i), msg);

#pragma GCC diagnostic ignored "-Wformat-truncation"
		snprintf(msg, sizeof(msg), "%s%x", msg, *(p + i));
	}
	tddPrint("%s\n", msg);
}

static void testFunction_str(uchar *p, char *name, uchar *input, int size)
{
	char msg[100] = "";

	sprintf(msg, "%s len : %d ", name, *p);
	for (int i = 0; i < size; i++) {
		TEST_ASSERT_EQUAL_CHAR_MESSAGE(*(input + i), *(p + i), msg);
		if (i > 0) {
#pragma GCC diagnostic ignored "-Wformat-truncation"
			snprintf(msg, sizeof(msg), "%s%c\n", msg, *(p + i));
		}
	}
	tddPrint("%s\n", msg);
}

static void testFunction(int i)
{
	memset(raw_data, 0, sizeof(raw_data));
	memset(&modem, 0, sizeof(Modem_t));
	memset(&conf, 0, sizeof(Config_t));
	memset(&modemCtx, 0, sizeof(ModemContext_t));

	char msg[100] = "";

	Modem_t *ptr = &inputData[i].modem;

	snprintf(modem.ctnStr, LEN_MODEM_CTN + 1, "%s", ptr->ctnStr);
	modem.modemQuality.cgi = ptr->modemQuality.cgi;
	modem.modemQuality.rsrp = ptr->modemQuality.rsrp;
	modem.modemQuality.snr = ptr->modemQuality.snr;
	parseQGMR(inputData[i].modemFwVer, &modem);
	modem.modemQuality.txPower = ptr->modemQuality.txPower;
	modemCtx.proc.initialReport = inputData[i].initialReport;
	conf.termModel = inputData[i].deviceFwVer;

	MODEM_qaData(raw_data);

	NbiotQaReportToLg_t *p = (NbiotQaReportToLg_t *)raw_data;

	sprintf(msg, "expect msg Version is %d", EXPECT_MSG_VER);
	TEST_ASSERT_EQUAL_INT_MESSAGE(inputData[i].expect.msgVer, p->msgVer, msg);
	testFunction_hex(p->ctn, "ctn test ", inputData[i].expect.ctn, sizeof(p->ctn));
	testFunction_hex(p->batt, "batt test ", inputData[i].expect.batt, sizeof(p->batt));
	testFunction_hex(p->cgi, "cgi test ", inputData[i].expect.cgi, sizeof(p->cgi));
	testFunction_hex(p->rsrp, "rsrp test ", inputData[i].expect.rsrp, sizeof(p->rsrp));
	testFunction_hex(p->sinr, "sinr test ", inputData[i].expect.sinr, sizeof(p->sinr));
	testFunction_str(p->model, "model Test ", inputData[i].expect.model, sizeof(p->model));
	testFunction_str(p->fwVer, "fwVer Test ", inputData[i].expect.fwVer, sizeof(p->fwVer));
	testFunction_hex(p->txPower, "txPower Test ", inputData[i].expect.txPower,
			 sizeof(p->txPower));

	// PASS
	TEST_ASSERT_EQUAL_HEX8(inputData[i].expect.location, p->location);
	TEST_ASSERT_EQUAL_HEX8(inputData[i].expect.neighborCell, p->neighborCell);

	TEST_ASSERT_EQUAL_HEX8(inputData[i].expect.ueInfo, p->ueInfo);
	tddPrint("ueInfo : %x\n", p->ueInfo);

	// PASS
	testFunction_hex(p->portInfo, "portInfo Test ", inputData[i].expect.portInfo,
			 sizeof(p->portInfo));

	testFunction_hex(p->reserved, "reserved Test ", inputData[i].expect.reserved,
			 sizeof(p->reserved));
}

static void coverageDummy()
{
	for (int i = 0; i < sizeof(coverage) / sizeof(coverageTest_t); i++) {
		conf.termModel = coverage[i].input;
		MODEM_qaData(raw_data);
	}
}

void test_tdd_MODEM_qaData(void)
{
	char msg[100] = "";
	for (int i = 0; i < sizeof(inputData) / sizeof(inputData_t); i++) {
		sprintf(msg, "Test Case %d", i);
		TEST_MESSAGE(msg);
		testFunction(i);
	}

	coverageDummy();
}

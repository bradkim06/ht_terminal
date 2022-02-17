#include <ctype.h>
#include <time.h>

#include "RTC.h"
#include "common_header.h"
#include "MSP430FlashUtil.h"

#include "port_desc.h"
#include "check_meter_misc.h"
#include "uart.h"
#include "lcdDriver.h"
#include "battery.h"
#include "osal_Timer.h"
#include "Task_Mgr.h"
#include "app.h"
#include "shell.h"
#include "test.h"
#include "mTest.h"
#include "flashDriver.h"
#include "meter.h"
#include "rtcAlarm.h"
#include "NFC_i2c.h"
#include "modem.h"
#include "uart.h"
#include "dataFlash.h"

#if (defined(NBIOT_LG_TYPE) || defined(NBIOT_BASE_TYPE))
#include "uuid.h"
#endif

#if defined(AUX_REPEATER)
#include "cc1200.h"
#include "rfTest.h"
#endif

extern Config_t conf;

static BOOL IsTestMode = FALSE;
static BOOL IsMeterTesting = FALSE;
static BOOL IsReedTesting = FALSE;
static uint8 TestTaskId = 0;

#define TEST_MODE
#undef TEST_MODE

/*
 ***************************************************************
    Test Mode Control functions
 ***************************************************************
 */
BOOL TEST_checkTestMode()
{
	// P1.0 --> if 0 with inernal pull-up, test mode
	//          else normal mode

	P1DIR |= 0x01;
	P1REN |= 0x01;
	P1OUT |= 0x01;

	P1DIR &= ~0x01;

	MISC_delayUs(1); // specific case need this delay

	IsTestMode = (P1IN & 0x01) ? FALSE : TRUE;

	P1DIR |= 0x01;
	P1OUT &= ~0x01;

#if defined(TEST_MODE)
	IsTestMode = TRUE;
#endif

	return IsTestMode;
}

BOOL TEST_isTestMode()
{
	return IsTestMode;
}

BOOL TEST_stop()
{
	uint8 c = SHELL_getChar();
	if (c == 'q' || c == 'Q') {
		return TRUE;
	} else {
		return FALSE;
	}
}

/*
 ***************************************************************
    Other functions
 ***************************************************************
 */
static BOOL saveTerminalConfig(const char *sn, const int ri, const int mi)
{
	BOOL valid = FALSE;

	do {
		if (strlen(sn) != SERIAL_NUM_LEN) {
			printf("length of s/n should be %d bytes\n", SERIAL_NUM_LEN);
			break;
		}
		if (ri <= 0 || ri > 24 || (24 % ri) != 0) {
			printf("wrong report interval(%d)\n", ri);
			break;
		}
		if (mi <= 0 || mi > 24 || (24 % mi) != 0 || mi > ri) {
			printf("metering interval(%d) is wrong or greater than report interval(%d)\n",
			       mi, ri);
			break;
		}
		valid = TRUE;
	} while (0);

	if (valid) {
#if defined(LORA_AS923_TYPE)
		if (!TEST_writeAppKeyUsingDEUI()) {
			return FALSE;
		}
#endif
		FLASH_readConfigInfo(&conf);

		memcpy(conf.serialNum, sn, SERIAL_NUM_LEN);
		conf.reportInterval = ri;
		conf.meterInterval = mi;
		conf.isShortInterval = 0;
#if NBIOT_DEVICE
		conf.isModemInit = FALSE;
		// Set default FOTA IP/Port and interval
		strcpy(conf.fotaIp, LG_DEFAULT_FOTA_SERVER_IP);
		conf.fotaPort = LG_DEFAULT_FOTA_SERVER_PORT;
		conf.fotaInterval = LG_DEFAULT_FOTA_DAY_INTERVAL;
#endif

		FLASH_saveConfigInfo(&conf);
	}

	return valid;
}

void TEST_config()
{
	Config_t config;
	FLASH_readConfigInfo(&config);

	APP_showConfig(&config);
}

void TEST_debugPrint(int flag)
{
	Config_t config;
	FLASH_readConfigInfo(&config);

	if (flag != 0xff) {
		if (flag) {
			config.debugPrint = 1;
		} else {
			config.debugPrint = 0;
		}
		FLASH_saveConfigInfo(&config);
	}
	printf("debug printing %s\n", config.debugPrint ? "on" : "off");
}

void TEST_meteringParamer(int ri, int mi)
{
	if (mi == 0 || 24 % mi || ri == 0 || 24 % ri) {
		printf("reportInterval and meteringInterval should be one of 1, 2, 3, 4, 6, 12, 24\n");
		return;
	}

	if (ri < mi) {
		printf("reportInterval(%d) is less than meteringInterval(%d)\n", ri, mi);
		return;
	}

	Config_t config;
	FLASH_readConfigInfo(&config);

	config.meterInterval = mi;
	config.reportInterval = ri;

	FLASH_saveConfigInfo(&config);

	printf("meteringInterval:%d, reportInterval:%d, intervalBaseTime:%d\n",
	       config.meterInterval, config.reportInterval, conf.intervalBaseTime);
}

void TEST_readRTC(int fromJig)
{
	Date_t date;

	RTC_read(&date);
	if (fromJig) {
		printf("trrtc %04d-%02d-%02d %02d:%02d:%02d\n", date.year, date.mon, date.day,
		       date.hour, date.min, date.sec);
	} else {
		printf("%04d-%02d-%02d %02d:%02d:%02d\n", date.year, date.mon, date.day, date.hour,
		       date.min, date.sec);
	}
}

void TEST_setRTC(int fromJig, int year, int mon, int day, int hour, int min, int sec)
{
	Date_t date;

	date.year = year;
	date.mon = mon;
	date.day = day;
	date.hour = hour;
	date.min = min;
	date.sec = sec;

	RTC_set(date);

	TEST_readRTC(fromJig);
}

void TEST_readFlash(int sector)
{
	uint8 flash[LEN_INFO_FLASH_SECTOR];

	if ((sector < 0 || sector > 3) && sector != 0xff) {
		printf("Invalid sector number[%d]\n", sector);
		return;
	}

	halIntState_t intState;
	uint8 start, end;

	if (sector == 0xff) {
		start = 0, end = 4;
	} else {
		start = sector, end = sector + 1;
	}

	extern const uint8 *FlashAddr[];
	for (int i = start; i < end; i++) {
		HAL_ENTER_CRITICAL_SECTION(intState);
		MSP430FLASH_read((uint8 *)FlashAddr[i], flash, LEN_INFO_FLASH_SECTOR);
		HAL_EXIT_CRITICAL_SECTION(intState);

		printf("\n======== Flash Memory - Sector[%d] =======\n", i);
		for (int j = 0; j < LEN_INFO_FLASH_SECTOR; j++) {
			if (j && !(j % 16)) {
				printf("\n");
			}
			printf("%02X ", flash[j]);
		}
		printf("\n");
	}
}

void TEST_eraseFlash(int sector)
{
	if ((sector < 0 || sector > 3)) {
		printf("Invalid sector number[%d]\n", sector);
		return;
	}

	halIntState_t intState;

	extern const uint8 *FlashAddr[];
	HAL_ENTER_CRITICAL_SECTION(intState);
	MSP430FLASH_erasePage((uint8 *)FlashAddr[sector]);
	HAL_EXIT_CRITICAL_SECTION(intState);
}

void TEST_readBattery()
{
	uint16 battery;

	printf("ACK\n");
	BATT_init();

	while (1) {
		uint32 startMs = TIMER_getMsec();
		do {
			if (TEST_stop() == TRUE) {
				return;
			}
		} while (TIMER_getMsecDiff(startMs) <= 1000);

		battery = BATT_getVoltage();
		printf("tbatt %d.%dV\n", battery / 10, battery % 10);
	}
}

void TEST_readSerialNum()
{
	Config_t config;
	FLASH_readConfigInfo(&config);

	printf("trsn ");
	for (int i = 0; i < SERIAL_NUM_LEN; i++) {
		printf("%c", config.serialNum[i]);
	}
	printf(" %s\n", FIRMWARE_VER);
}

void TEST_writeSerialNum(char *serialNum)
{
	if (strlen(serialNum) != SERIAL_NUM_LEN) {
		printf("serial number must be %d digits\n", SERIAL_NUM_LEN);
		return;
	}

	Config_t config;
	FLASH_readConfigInfo(&config);

	for (int i = 0; i < SERIAL_NUM_LEN; i++) {
		config.serialNum[i] = *serialNum++;
	}

	FLASH_saveConfigInfo(&config);

	TEST_readSerialNum();
}

static void reedResponse()
{
	if (IsReedTesting)
		printf("treed OK\n");
}

void TEST_reedSensor()
{
	printf("ACK\n");
	AppTaskId = TestTaskId;

	IsReedTesting = TRUE;
	while (1) {
		MISC_delayMs(100);
		if (TEST_stop() == TRUE) {
			break;
		}
	}
	IsReedTesting = FALSE;
}

void TEST_readMeter(int type)
{
	AppTaskId = TestTaskId;

	if (type == 0xff) {
		printf("Usage : rmeter [TYPE]\n");
		printf("[TYPE] list\n");
		printf("  1 : Water STD\n");
		printf("  2 : Water SH\n");
		printf("  3 : Water SH Big\n");
		printf("  4 : Water MNS\n");
		printf("  5 : Calorie OneTL\n");
		printf("  6 : Water OneTL\n");
		printf("  7 : Gas OneTL\n");
		printf("  8 : HotWater OneTL\n\n");
		return;
	}

	int meterType;
	if (type == 1) {
		meterType = W_STANDARD_D;
	} else if (type == 2) {
		meterType = W_SHINHAN_D;
	} else if (type == 3) {
		meterType = W_SHINHAN_D_BIG;
	} else if (type == 4) {
		meterType = W_MNS_D;
	} else if (type == 5) {
		meterType = C_ONETL_D;
	} else if (type == 6) {
		meterType = W_ONETL_D;
	} else if (type == 7) {
		meterType = G_ONETL_D;
	} else if (type == 8) {
		meterType = H_ONETL_D;
	} else {
		printf("Unknown type : %02X\n", type);
		return;
	}

	int nLoop = 0;
	while (++nLoop) {
		METER_sendRequest(meterType);

		IsMeterTesting = TRUE;
		uint32 ms = TIMER_getMsec();
		while (TIMER_getMsecDiff(ms) < (2 * 1000)) {
			if (TEST_stop()) {
				goto TEST_RMETER_FINISH;
			} else {
				TEST_runTasks();
			}
			if (!IsMeterTesting)
				break;
		}

		int result;
		MeterUnitData_t unit;
		memset(&unit, 0xFF, sizeof(MeterUnitData_t));
		if (IsMeterTesting) {
			result = FAIL;
		} else {
			result = METER_recvResponse(meterType, &unit);
		}

		if (meterType == C_ONETL_D) {
			printf_ts(
				"[%s] try:%d result:%s flow:%02x%02x%02x%02x data:%02x%02x%02x%02x(ST:%02d.%02d/RT:%02d.%02d)\n",
				METER_getMeterName(meterType), nLoop,
				((result == SUCCESS) ? "pass" : "fail"), unit.flowData[0],
				unit.flowData[1], unit.flowData[2], unit.flowData[3],
				unit.meterData[0], unit.meterData[1], unit.meterData[2],
				unit.meterData[3], unit.st[0], unit.st[1], unit.rt[0], unit.rt[1]);
		} else {
			printf_ts("[%s] try:%d result:%s data:%02x%02x%02x%02x(%02x)\n",
				  METER_getMeterName(meterType), nLoop,
				  ((result == SUCCESS) ? "pass" : "fail"), unit.meterData[0],
				  unit.meterData[1], unit.meterData[2], unit.meterData[3],
				  unit.meterStatus);
		}
	}

TEST_RMETER_FINISH:
	IsMeterTesting = FALSE;
	METER_disable();

	LCD_init();
	LCD_displayString("TEST");
}

void TEST_writeMeter(char *valueStr)
{
	char value[12];
	memset(value, '0', 12);

	int valid = 0;
	do {
		char *pStart = valueStr;
		char *pDot = strstr(pStart, ".");
		if (pDot == NULL) {
			break;
		}

		char *p = pDot - 1;
		for (int i = 5; i >= 0; i--) {
			if (p < pStart) {
				break;
			}
			value[i] = *p--;
		}

		p = pDot + 1;
		for (int i = 6; i < 11; i++) {
			if (*p == '\r' || *p == '\n' || *p == '\0') {
				break;
			}
			value[i] = *p++;
		}

		valid = 1;

		for (int i = 0; i < 12; i++) {
			if (isxdigit(value[i]) == 0) {
				valid = 0;
				break;
			}
		}
	} while (0);

	if (valid == 0) {
		printf("invalid value(%s) try again with xxxxxx.yyyyy format\n", valueStr);
		return;
	}

	typedef struct {
		uint8 stx;
		uint8 cfield;
		uint8 afield;
		uint8 value[6];
		uint8 checksum;
		uint8 etx;
	} set_meter_value_t;

	set_meter_value_t txMsg;
	set_meter_value_t *pMsg = &txMsg;
	memset(pMsg, 0, sizeof(set_meter_value_t));

	pMsg->stx = STD_REQ_START;
	pMsg->cfield = 0xA1;
	pMsg->afield = STD_REQ_ADDR;
	for (int i = 0; i < 6; i++) {
		pMsg->value[i] = ascii2BCD(value[10 - 2 * i + 0], value[10 - 2 * i + 1]);
	}
	pMsg->checksum = pMsg->cfield + pMsg->afield;
	for (int i = 0; i < 6; i++) {
		pMsg->checksum += pMsg->value[i];
	}
	pMsg->etx = STD_REQ_STOP;

	uint8 *p = (uint8 *)pMsg;

	printf("set meter value: ");
	for (int i = 0; i < sizeof(set_meter_value_t); i++) {
		printf(" %02X", *(p + i));
	}
	printf("\n");

	METER_enable(W_STANDARD_D);
	MISC_delayMs(20);
	UART_send(UART_A3, &txMsg, sizeof(set_meter_value_t), FALSE);

	MISC_delayMs(1500);
	METER_disable();
}

void TEST_lpm3(int mode)
{
	printf("ACK\n");

	Config_t config;
	FLASH_readConfigInfo(&config);
	if (mode == 1) {
		config.sleepMode = 0;
	} else if (mode == 0) {
		config.sleepMode = 1;
	}
	FLASH_saveConfigInfo(&config);

	METER_disable();
	MODEM_disable();

	P1DIR = 0xFF;
	P1OUT = 0x00;
	P1OUT |= BM(PORT_LCD_SWITCH);

	LCD_init();
	LCD_wait();

	NFC_fdDisable();

	LCD_disable();
	PRINT_disable();

#if defined(AUX_REPEATER)
	CC1200_setSleep();
	CC1200_rfOff();
#endif

	MODEM_turnOff();

	device_sleep_state = FALSE;
	GLOBAL_DISABLE_INT();
	SLEEP_DEVICE();

	while (1)
		;
}

void TEST_lpm2()
{
	printf("ACK\n");

	Config_t config;
	FLASH_readConfigInfo(&config);
	config.sleepMode = 1;
	FLASH_saveConfigInfo(&config);

	METER_disable();
	MODEM_disable();

	P1DIR = 0xFF;
	P1OUT = 0x00;
	P1OUT |= BM(PORT_LCD_SWITCH);

	LCD_init();
	LCD_wait();

	NFC_fdDisable();

	TIMER_stop();
	LCD_disable();
	PRINT_disable();

	MODEM_turnOff();

#if defined(AUX_REPEATER)
	CC1200_setSleep();
	CC1200_rfOff();
#endif

	device_sleep_state = FALSE;
	GLOBAL_DISABLE_INT();
	SET_ACTIVE_OFF_LPM(TRUE);

	while (1)
		;
}

void TEST_readFwVersion()
{
	printf("tver %s\n", FIRMWARE_VER);
}

void TEST_lcd()
{
	LCD_init();
	printf("ACK\n");

	while (1) {
		LCD_displayAll();
		MISC_delayMs(800);
		LCD_displayFirmwareVersion();
		MISC_delayMs(500);
		if (TEST_stop() == TRUE) {
			break;
		}
	}
	LCD_displayString("TEST");
}

void TEST_meterLCD()
{
	if (METER_std_lcdTestReq()) {
		printf("ACK\n");
	}
}

void TEST_sleepMode(int flag)
{
	Config_t config;
	FLASH_readConfigInfo(&config);

	if (flag != 0xff) {
		config.sleepMode = flag ? 1 : 0;
		FLASH_saveConfigInfo(&config);
	}

	if (config.sleepMode) {
		printf("sleep mode\n");
	} else {
		printf("active mode\n");
	}
}

void TEST_riCtrlMode(int flag)
{
	Config_t config;
	FLASH_readConfigInfo(&config);

	if (flag != 0xff) {
		config.riCtrlMode = flag ? 1 : 0;
		config.riCtrlChgCount = 0;
		config.riCtrlValue = config.reportInterval;
		FLASH_saveConfigInfo(&config);
	}

	printf("report control %s\n", config.riCtrlMode ? "on" : "off");
}

void TEST_shortInterval(int flag)
{
	Config_t config;
	FLASH_readConfigInfo(&config);

	if (flag != 0xff) {
		config.isShortInterval = flag ? 1 : 0;
		FLASH_saveConfigInfo(&config);
	}

	printf("short interval %s\n", config.isShortInterval ? "on" : "off");
	return;
}

void TEST_setMeterType(int type)
{
	Config_t config;
	FLASH_readConfigInfo(&config);

	if (type == 0xff) {
		printf("Usage : mt [TYPE]\n");
		printf("[TYPE] list\n");
		printf("  1 : Water STD\n");
		printf("  2 : Water SH\n");
		printf("  3 : Water SH Big\n");
		printf("  4 : Water MNS\n");
		printf("  5 : Calorie OneTL\n");
		printf("  6 : Water OneTL\n");
		printf("  7 : Gas OneTL\n");
		printf("  8 : HotWater OneTL\n\n");
		return;
	}

	if (type == 1) {
		config.meterType = W_STANDARD_D;
	} else if (type == 2) {
		config.meterType = W_SHINHAN_D;
	} else if (type == 3) {
		config.meterType = W_SHINHAN_D_BIG;
	} else if (type == 4) {
		config.meterType = W_MNS_D;
	} else if (type == 5) {
		config.meterType = C_ONETL_D;
	} else if (type == 6) {
		config.meterType = W_ONETL_D;
	} else if (type == 7) {
		config.meterType = G_ONETL_D;
	} else if (type == 8) {
		config.meterType = H_ONETL_D;
	} else {
		printf("Unknown type : %02X\n", type);
		return;
	}

	FLASH_saveConfigInfo(&config);
}

void TEST_pwsn(char *sn, int ri, int mi)
{
	if (saveTerminalConfig(sn, ri, mi) != TRUE) {
		printf("trsn ERR\n");
		return;
	}

	TEST_prsn();
}

void TEST_prsn()
{
	Config_t config;

	FLASH_readConfigInfo(&config);

	char serial[SERIAL_NUM_LEN + 1] = "";
	memcpy(serial, config.serialNum, SERIAL_NUM_LEN);
	serial[SERIAL_NUM_LEN] = '\0';

	printf("trsn %s %s %d %d\n", serial, FIRMWARE_VER, config.reportInterval,
	       config.meterInterval);
}

void TEST_pwmeter(int numMeter, int type1, int port1, int type2, int port2, int type3, int port3)
{
	FLASH_readConfigInfo(&conf);

	conf.meterType = type1;
	FLASH_saveConfigInfo(&conf);

	MISC_delayMs(1000); // 생산공정에서 모뎀 BIP완료 메세지 대기를 위한 딜레이
	printf("trmeter %d %d %d %d %d %d %d\n", numMeter, type1, port1, type2, port2, type3,
	       port3);
}

void TEST_pmeter(int numMeter, int type1, int port1, int type2, int port2, int type3, int port3)
{
	FLASH_readConfigInfo(&conf);

	conf.meterType = type1;
	FLASH_saveConfigInfo(&conf);

	METER_sendRequest(conf.meterType);

	MeterUnitData_t unit;
	int result = FAIL;

	IsMeterTesting = TRUE;
	uint32 ms = TIMER_getMsec();
	while (TIMER_getMsecDiff(ms) < (3 * 1000)) {
		TEST_runTasks();
		if (IsMeterTesting) {
			MISC_delayMs(1);
		} else {
			result = METER_recvResponse(conf.meterType, &unit);
			if (result == SUCCESS) {
				break;
			}
		}
	}

	if (result == SUCCESS) {
		printf("tmeter 1 %02x%02x%02x%02x\n", unit.meterData[0], unit.meterData[1],
		       unit.meterData[2], unit.meterData[3]);
	} else {
		printf("tmeter 1 ERR\n");
	}

	IsMeterTesting = FALSE;
	METER_disable();

	LCD_init();
	LCD_displayString("TEST");
}

void TEST_pmwsn(char *sn, int ri, int mi, int caliber, int q3, int qt, int q2, int q1, int maker)
{
	if (saveTerminalConfig(sn, ri, mi) != TRUE) {
		printf("tmrsn ERR\n");
		return;
	}

	char meter_sn[4];
	meter_sn[3] = ascii2BCD(sn[4], sn[5]);
	meter_sn[2] = ascii2BCD(sn[6], sn[7]);
	meter_sn[1] = ascii2BCD(sn[8], sn[9]);
	meter_sn[0] = ascii2BCD(sn[10], sn[11]);
	if (METER_std_adjustReq(meter_sn, caliber, maker, q3, qt, q2, q1) != TRUE) {
		return;
	}

	IsMeterTesting = TRUE;
	uint32 ms = TIMER_getMsec();
	while (TIMER_getMsecDiff(ms) < (3 * 1000)) {
		TEST_runTasks();
		if (IsMeterTesting) {
			MISC_delayMs(1);
		} else {
			StdAdjustResult_t result;
			if (METER_std_adjustResp(&result)) {
				printf("tmrsn %s %s %d %d ", sn, FIRMWARE_VER, conf.reportInterval,
				       conf.meterInterval);
				uint16 recv_q3, recv_qt, recv_q2, recv_q1;
				recv_q3 = result.q3[0] | result.q3[1] << 8;
				recv_qt = result.qt[0] | result.qt[1] << 8;
				recv_q2 = result.q2[0] | result.q2[1] << 8;
				recv_q1 = result.q1[0] | result.q1[1] << 8;
				// 사실 40, 50mm은 소형/대형이 혼재하지만 따로 구분할 방법이 없어 50mm까진 소형으로 고려한다.
				if (caliber > MT_50mm) {
					printf("%d %d %d %d %d %d %d %d\n", result.caliber, recv_q3,
					       recv_qt, recv_q2, recv_q1, result.maker,
					       result.fwVersion, result.errorCode);
				} else {
					printf("%d %d.%d %d.%d %d.%d %d.%d %d %d %d\n",
					       result.caliber, recv_q3 / 10, recv_q3 % 10,
					       recv_qt / 10, recv_qt % 10, recv_q2 / 10,
					       recv_q2 % 10, recv_q1 / 10, recv_q1 % 10,
					       result.maker, result.fwVersion, result.errorCode);
				}
			}
			break;
		}
	}
	IsMeterTesting = FALSE;
	METER_disable();

	LCD_init();
	LCD_displayString("TEST");
}

void TEST_pmrsn(BOOL isJigTest)
{
	if (METER_std_configRequest() != TRUE) {
		return;
	}

	IsMeterTesting = TRUE;
	uint32 ms = TIMER_getMsec();
	while (TIMER_getMsecDiff(ms) < (3 * 1000)) {
		TEST_runTasks();
		if (IsMeterTesting) {
			MISC_delayMs(1);
		} else {
			StdAdjustResult_t result;
			if (METER_std_adjustResp(&result)) {
				uint16 recv_q3, recv_qt, recv_q2, recv_q1;
				recv_q3 = result.q3[0] | result.q3[1] << 8;
				recv_qt = result.qt[0] | result.qt[1] << 8;
				recv_q2 = result.q2[0] | result.q2[1] << 8;
				recv_q1 = result.q1[0] | result.q1[1] << 8;
				if (isJigTest) {
					Config_t config;
					FLASH_readConfigInfo(&config);

					char serial[SERIAL_NUM_LEN + 1] = "";
					memcpy(serial, config.serialNum, SERIAL_NUM_LEN);
					serial[SERIAL_NUM_LEN] = '\0';

					printf("tmrsn %s %s %d %d ", serial, FIRMWARE_VER,
					       config.reportInterval, config.meterInterval);
					// 사실 40, 50mm은 소형/대형이 혼재하지만 따로 구분할 방법이 없어 50mm까진 소형으로 고려한다.
					if (result.caliber > MT_50mm) {
						printf("%d %d %d %d %d %d %d %d\n", result.caliber,
						       recv_q3, recv_qt, recv_q2, recv_q1,
						       result.maker, result.fwVersion,
						       result.errorCode);
					} else {
						printf("%d %d.%d %d.%d %d.%d %d.%d %d %d %d\n",
						       result.caliber, recv_q3 / 10, recv_q3 % 10,
						       recv_qt / 10, recv_qt % 10, recv_q2 / 10,
						       recv_q2 % 10, recv_q1 / 10, recv_q1 % 10,
						       result.maker, result.fwVersion,
						       result.errorCode);
					}
				} else {
					if (result.caliber > MT_50mm) {
						printf("S/N(%02x%02x%02x%02x) F/W(%d) caliber(%02x) Q3(%d) Qt(%d) Q2(%d) Q1(%d)\n",
						       result.serial[3], result.serial[2],
						       result.serial[1], result.serial[0],
						       result.fwVersion, result.caliber, recv_q3,
						       recv_qt, recv_q2, recv_q1);
					} else {
						printf("S/N(%02x%02x%02x%02x) F/W(%d) caliber(%02x) Q3(%d.%d) Qt(%d.%d) Q2(%d.%d) Q1(%d.%d)\n",
						       result.serial[3], result.serial[2],
						       result.serial[1], result.serial[0],
						       result.fwVersion, result.caliber,
						       recv_q3 / 10, recv_q3 % 10, recv_qt / 10,
						       recv_qt % 10, recv_q2 / 10, recv_q2 % 10,
						       recv_q1 / 10, recv_q1 % 10);
					}
				}
			}
			break;
		}
	}
	IsMeterTesting = FALSE;
	METER_disable();

	LCD_init();
	LCD_displayString("TEST");
}

void TEST_resetNFC()
{
	printf("ACK\n");

	NFC_tagDisable();
	NFC_fdDisable();

	//VCC control 사용전 보드 => 추후 삭제
	PORT1_DIR |= BM(PORT_NFC_TAG);
	PORT1_OUT |= BM(PORT_NFC_TAG);

	MISC_delayMs(1000); // 생산공정에서 모뎀 BIP완료 메세지 대기를 위한 딜레이
	if (NFC_factoryResetTag() == TRUE) {
		printf("tnfc OK\n");
	} else {
		printf("tnfc ERR\n");
	}

	//VCC control 사용전 보드 => 추후 삭제
	PORT1_DIR &= ~BM(PORT_NFC_TAG);
	NFC_init();
}

void TEST_checkNFC()
{
	uint8 org_AppTaskId = AppTaskId;
	AppTaskId = TestTaskId;

	printf("ACK\n");

	while (1) {
		if (TEST_stop() == TRUE) {
			break;
		}
	}

	AppTaskId = org_AppTaskId;
}

/*
 ***************************************************************
    Test process and task
 ***************************************************************
 */
static event32_t interruptTask(uint8 taskId, event32_t events)
{
	if (events & APP_EVENT_SENSOR_REED) {
		reedResponse();
		return (events ^ APP_EVENT_SENSOR_REED);
	}

	if (events & APP_EVENT_MODEM_RX) {
		TEST_modemResponse();
		return (events ^ APP_EVENT_MODEM_RX);
	}

	if (events & APP_EVENT_METER_RX) {
		METER_disable();
		IsMeterTesting = FALSE;
		return (events ^ APP_EVENT_METER_RX);
	}

	return 0;
}

const pTaskEventHandlerFn TestTasksArray[] = { interruptTask,
#if defined(AUX_REPEATER)
					       CC1200_tasks
#endif
};

const uint8 testTasksCnt = sizeof(TestTasksArray) / sizeof(TestTasksArray[0]);
extern event32_t *TasksEvents;

void TEST_runTasks()
{
	uint8 idx = 0;

	WDTCTL = WDTPW | WDTCNTCL; // Clear watchdog timer
	WDTCTL = WDT_ARST_16SEC; // Start watchdog timer(16 sec)

	for (int i = 0; i < (testTasksCnt + 1); i++) {
		if (++idx >= testTasksCnt) {
			idx = 0;
		}

		do {
			if (TasksEvents[idx]) { // Task is highest priority that is ready.
				break;
			}
		} while (++idx < testTasksCnt);

		if (idx < testTasksCnt) {
			halIntState_t intState;
			event32_t events;

			HAL_ENTER_CRITICAL_SECTION(intState);
			events = TasksEvents[idx];
			TasksEvents[idx] = 0;
			HAL_EXIT_CRITICAL_SECTION(intState);

			//yikim 2017.09.06 NFC TEST중 발생하여 처리
			if (events == 0xFFFFFFFF) {
				__no_operation();
			} else {
				events = (TestTasksArray[idx])(idx, events);

				HAL_ENTER_CRITICAL_SECTION(intState);
				TasksEvents[idx] |= events;
				HAL_EXIT_CRITICAL_SECTION(intState);
			}
		}
	}
}

void TEST_run()
{
	WDTCTL = WDTPW | WDTCNTCL; // Clear watchdog timer
	WDTCTL = WDT_ARST_16SEC; // Start watchdog timer(16 sec)
	PRINT_enable();

	TasksEvents = (event32_t *)malloc(sizeof(event32_t) * testTasksCnt);
	if (TasksEvents == NULL) {
		printf("malloc error - TASKMGR_init\n");
		exit(1);
	}
	memset(TasksEvents, 0, (sizeof(event32_t) * testTasksCnt));

	BATT_init();
	NFC_init();
	LCD_init();
	LCD_displayString("TEST");

	//task 초기화
	AppTaskId = TestTaskId;
#if defined(AUX_REPEATER)
	// 테스트를 위한 Radio task 동작이 필요하므로 초기화.
	CC1200_init(TestTaskId + 1);
#endif

	RTC_init();
	FLASH_readConfigInfo(&conf);
	FLASH_updateResetCount(&conf);

	PRINT_stop();
	TEST_initModem(TestTaskId);
	PRINT_resume();

	printf("\n\n\n\n");
	printf("===========================================\n");
	printf("             TEST MODE ");
#if LORA_DEVICE
	printf("(Solu-M)\n");
#else // NBIOT_DEVICE
	printf("(NB-IoT)\n");
#endif
	printf("   Compiled at (%s / %s)\n", __DATE__, __TIME__);
	printf("===========================================\n");
	printf("\n");

#if !defined(AUX_REPEATER)
	dataFlash_init();
#endif

	SHELL_init();
	SHELL_run();

	printf("goto normal mode ... wait a moment\n");
	REBOOT_SYSTEM();
}

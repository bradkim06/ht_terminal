#include <msp430.h>
#include <ctype.h>
#include <time.h>

#include "common_header.h"
#include "port_desc.h"
#include "uart.h"
#include "check_meter_misc.h"

#include "osal_Timer.h"
#include "app.h"
#include "meter.h"
#include "lcdDriver.h"
#include "shell.h"
#include "rtcAlarm.h"
#include "modem.h"
#include "message.h"
#include "battery.h"
#include "test.h"

extern Config_t conf;
extern uint8 AppProcess;

Modem_t modem;
ModemFlag_t mFlag;

void MODEM_deleteLastAccessTime()
{
	memset(&modem.lastAccessTime, 0, sizeof(Date_t));
}

void MODEM_copyLastAccessTime(uint8 *p)
{
	*(p + 0) = modem.lastAccessTime.year % 0x100;
	*(p + 1) = modem.lastAccessTime.year / 0x100;
	*(p + 2) = modem.lastAccessTime.mon;
	*(p + 3) = modem.lastAccessTime.day;
	*(p + 4) = modem.lastAccessTime.hour;
	*(p + 5) = modem.lastAccessTime.min;
	*(p + 6) = modem.lastAccessTime.sec;
}

void MODEM_write(char *pCmd)
{
	if (TEST_isTestMode() == FALSE) {
		printf(TP_ANSI_FG_BLUE);
		printf("%s\n", pCmd);
		printf(TP_ANSI_RESET);
	}

	int len = strlen(pCmd);
	UART_send(UART_A2, pCmd, len, TRUE);
}

static void send_dummyCommand()
{
	// 모뎀으로부터 어떤 응답이든 받기 위한 목적에서 사용되며, 꼭 ADR이 아니라도 상관 없음
	MODEM_write("AT+ADR");
}

void displayAccessResult(int error)
{
	modem.lastError = error;
	if (AppProcess == APP_IMMEDIATE_REPORT || AppProcess == APP_INITIAL_REPORT) {
		char buf[0x10] = "";

		if (error == ERROR_CODE_NONE) {
			snprintf(buf, 0x10, "SUCCESS");
		} else {
			snprintf(buf, 0x10, "FAIL %d", error);
		}
		LCD_displayString(buf);

		MISC_delayMs(1000);
		LCD_redraw();
	}
}

void MODEM_timeout()
{
	if (AppProcess == APP_IMMEDIATE_REPORT || AppProcess == APP_INITIAL_REPORT) {
		int error = ERROR_CODE_UNKNOWN;
		switch (modem.step) {
		case MODEM_STEP_IDLE:
			error = ERROR_CODE_NONE;
			break;
		case MODEM_STEP_CHECK_ALIVE:
		case MODEM_STEP_GET_APP_EUI:
		case MODEM_STEP_GET_DEV_EUI:
			error = ERROR_CODE_MODEM;
			break;
		case MODEM_STEP_WAIT_JOIN:
			error = ERROR_CODE_NOT_JOINED;
			break;
		case MODEM_STEP_WAIT_ACK_DATA:
		case MODEM_STEP_GET_RSSI:
		case MODEM_STEP_GET_SNR:
			error = ERROR_CODE_COMM;
			break;
		case MODEM_STEP_GET_TIME:
			error = ERROR_CODE_GET_TIME;
			break;
		}

		if (error == ERROR_CODE_MODEM || error == ERROR_CODE_NOT_JOINED ||
		    error == ERROR_CODE_COMM) {
			modem.failCount++;
			// TODO : It may be necessary to restart the JOIN or to change the DR.
			//        (If the number of failures continuously exceeds)
		} else {
			// ERROR_CODE_GET_TIME의 경우 데이터 통신에는 성공한 것이므로 연속 실패가 중단된 것으로 판단
			modem.failCount = 0;
		}

		modem.step = MODEM_STEP_IDLE;
		displayAccessResult(error);
	}
	OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_POWER_OFF);
}

void MODEM_checkAlive()
{
	OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_CHECK_ALIVE);

	if (modem.step != MODEM_STEP_IDLE && mFlag.alive == 0) {
		send_dummyCommand();
		OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_CHECK_ALIVE, (uint32)2000);
	}
}

void MODEM_waitJoin()
{
	/*
     AS923의 경우 아래와 같은 이유로 해당 동작을 생략한다.
     1. AT command(AT+PS)를 지원하지 않아 Join여부를 매번확인 할 수 없음.
     2. LoRaWAN spec을 따르기 때문에 Join과정이 간단하여 실패/성공 여부와 관계없이 빠름.
     다만, SKT LoRa와의 호환성 유지를 위해 해당 함수를
    */
}

void MODEM_reset()
{
	// reset LoRa Modem: NRESET(P2.3)
	P2SEL &= ~0x08;
	P2DIR |= 0x08;

	P2OUT &= ~0x08;
	MISC_delayMs(500);
	P2OUT |= 0x08;
}

void resetLow()
{
	P2SEL &= ~0x08;
	P2DIR |= 0x08;
	P2OUT &= ~0x08;
}

void resetHigh()
{
	P2SEL &= ~0x08;
	P2DIR |= 0x08;
	P2OUT |= 0x08;
}

void MODEM_turnOff()
{
	// RF_PWR_CON(P2.0)
	P2SEL &= ~0x01;
	P2DIR |= 0x01;
	P2OUT |= 0x01;

	resetLow();
}

void MODEM_turnOn()
{
	// RF_PWR_CON(P2.0)
	P2SEL &= ~0x01;
	P2DIR |= 0x01;
	P2OUT &= ~0x01;

	resetHigh();
}

void MODEM_initialization()
{
	// LoRa의 경우 최초 Join 후 추가 Join 동작을 생략하기 위해 Power off를 수행하지 않는다.
	// 그러므로 Power on 동작을 "Start Access"에서 하지 않고 초기화 시점에 한번만 수행한다.
	MODEM_turnOn();
	MODEM_reset();

	memset(&modem, 0, sizeof(modem));
	modem.step = MODEM_STEP_IDLE;
}

void MODEM_enable()
{
	UartInitParam_t param = { .isTxEnable = TRUE,
				  .isRxEnable = TRUE,
				  .baudrate = UART_BAUDRATE_38400,
				  .parity = UART_PARITY_NONE,
				  .stopbit = UART_STOPBIT_ONE,
				  .charLen = UART_8BITS_CHAR };

	UART_open(UART_A2, &(param));
}

void MODEM_disable()
{
	UART_close(UART_A2, GPIO_INPUT);
}

void MODEM_open(int process)
{
	AppProcess = process;

	if (modem.step != MODEM_STEP_IDLE) {
		return;
	}

	MODEM_enable();
	memset(&mFlag, 0, sizeof(mFlag));

	if (AppProcess == APP_INITIAL_REPORT || modem.joined == 0) {
		MODEM_reset();
		MISC_delayMs(2000);
	}

	if (AppProcess == APP_IMMEDIATE_REPORT || AppProcess == APP_INITIAL_REPORT) {
		LCD_accessModem();
	}

	modem.step = MODEM_STEP_CHECK_ALIVE;

	MODEM_checkAlive();

	OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT, (uint32)60000);
}

void MODEM_close()
{
	MODEM_disable();

	OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT);
	OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_RX);
	OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_POWER_OFF);
	OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_CHECK_ALIVE);

	modem.step = MODEM_STEP_IDLE;
	AppProcess = APP_IDLE;

	RTC_read(&modem.lastAccessTime);
}

void MODEM_sendData(uint8 *data, int len)
{
	char buf[MODEM_TX_BUF_LEN] = "";
	snprintf(buf, MODEM_TX_BUF_LEN, "%s", "AT+SEND 02");

	if (data != NULL) {
		for (int i = 0; i < len; i++) {
			snprintf(buf, MODEM_TX_BUF_LEN, "%s%02X", buf, data[i]);
		}
	}
	MODEM_write(buf);
}

void extractEUI(char *p, char *str)
{
	int nDigit = 0;
	while (nDigit < 16) {
		if (isxdigit(*p)) {
			*(str + nDigit++) = *p;
		}
		p++;
	}
}

void parse_appEui(char *p)
{
	char str[16];

	extractEUI(p, str);
	for (int i = 0; i < 8; i++) {
		modem.appEui[i] = ascii2BCD(str[2 * i + 0], str[2 * i + 1]);
	}

	printf("My AppEui: %02X%02X%02X%02X%02X%02X%02X%02X\n", modem.appEui[0], modem.appEui[1],
	       modem.appEui[2], modem.appEui[3], modem.appEui[4], modem.appEui[5], modem.appEui[6],
	       modem.appEui[7]);
}

void parse_devEui(char *p)
{
	char str[16];

	extractEUI(p, str);
	for (int i = 0; i < 8; i++) {
		modem.devEui[i] = ascii2BCD(str[2 * i + 0], str[2 * i + 1]);
	}

	if (memcmp(conf.devEui, modem.devEui, 8) != 0) {
		memcpy(conf.devEui, modem.devEui, 8);
		FLASH_saveConfigInfo(&conf);

		printf_ts("devEui changed\n");

		printf("New DevEui: %02X%02X%02X%02X%02X%02X%02X%02X\n", modem.devEui[0],
		       modem.devEui[1], modem.devEui[2], modem.devEui[3], modem.devEui[4],
		       modem.devEui[5], modem.devEui[6], modem.devEui[7]);
	}
}

void MODEM_read()
{
	char buf[MODEM_RX_BUF_LEN + 1]; // 1 for margin
	memset(buf, 0, MODEM_RX_BUF_LEN + 1);
	int len = UART_receive(UART_A2, buf, MODEM_RX_BUF_LEN);

	if (TEST_isTestMode() == FALSE) {
		printf(TP_ANSI_FG_RED);
		PRINT_string(buf, len);
		printf(TP_ANSI_RESET);
	}

	SOLUM_MODEM_parse(buf, len);

	OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
}

void MODEM_normalTerminaltion()
{
	OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT);
	OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_POWER_OFF);
	displayAccessResult(ERROR_CODE_NONE);
	modem.step = MODEM_STEP_IDLE;
	modem.failCount = 0;
}

void goto_getDevEui()
{
	mFlag.gotDevEui = 0;
	modem.step = MODEM_STEP_GET_DEV_EUI;

	MODEM_write("AT+DEUI");

	OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT, (uint32)10000);
}

void goto_getAppEui()
{
	mFlag.gotAppEui = mFlag.gotDevEui = 0;

	modem.step = MODEM_STEP_GET_APP_EUI;

	MODEM_write("AT+AEUI");

	OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT, (uint32)10000);
}

void goto_waitJoin()
{
	modem.step = MODEM_STEP_WAIT_JOIN;
	// TODO : If 60sec wait is not enough for the JOIN, need to adjustment later.
	OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT, (uint32)60000);

	send_dummyCommand();
}

static void goto_sendAndWaitAck()
{
	mFlag.acked = 0;
	mFlag.timeoutOccurred = 0;

	if (AppProcess == APP_IMMEDIATE_REPORT || AppProcess == APP_INITIAL_REPORT) {
		MODEM_sendJoin();
	} else {
		MODEM_sendDataReport();
	}

	modem.timeoutCount = 0;
	modem.step = MODEM_STEP_WAIT_ACK_DATA;
	OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT, (uint32)60000);
}

#if 0
// 현재 시간동기화를 위한 시간데이터 요청은 NS에서 지원해주지 않기 때문에
// Disable 하였으며, 추후 다시 고려해야 함.
static void goto_getTime()
{
    if(RTC_needTimeSync() == FALSE) {
        MODEM_normalTerminaltion();
        return;
    }

    mFlag.gotTime = 0;
    modem.step = MODEM_STEP_GET_TIME;
    MODEM_write("AT+SEND A0"); // Send void uplink(FPort = A0h) for get data from NS.
    OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT, (uint32)60000);
}
#endif

static void handle_timeout()
{
	OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT);

	mFlag.timeoutOccurred = 0;
	modem.timeoutCount++;

	printf_ts("timeout(%d)\n", modem.timeoutCount);

	if (AppProcess == APP_IMMEDIATE_REPORT || AppProcess == APP_INITIAL_REPORT) {
		// 초기설치시(APP_INITIAL_REPORT)나 점검시(APP_IMMEDIATE_REPORT)에만 상황 변화를 LCD에 표시
		LCD_redraw();
		MISC_delayMs(100);
		LCD_accessModem();
	}

	OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT, (uint32)60000);
}

void display_rssiSnr()
{
	if (AppProcess == APP_IMMEDIATE_REPORT || AppProcess == APP_INITIAL_REPORT) {
		if (mFlag.gotSnr && mFlag.gotRssi) {
			char rssiSNR[0x10] = "";
			memset(rssiSNR, ' ', 0x10);
			sprintf(rssiSNR, "-%ddB    ", modem.lastRSSI);
			sprintf(rssiSNR + 7, "%d", modem.lastSNR);
			LCD_displayString(rssiSNR);
			MISC_delayMs(1000);
		}
	}
}

void MODEM_process()
{
	// APP_EVENT_MODEM_POWER_OFF는 정상 처리 후의 종료
	// APP_EVENT_MODEM_TIMEOUT은 실패로 인한 종료

	switch (modem.step) {
	case MODEM_STEP_CHECK_ALIVE:
		if (mFlag.alive) {
			OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT);
			if (AppProcess == APP_INITIAL_REPORT || modem.joined == 0) {
				goto_getAppEui();
			} else {
				goto_sendAndWaitAck();
			}
		}
		break;

	case MODEM_STEP_GET_APP_EUI:
		if (mFlag.gotAppEui) {
			OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT);
			goto_getDevEui();
		}
		break;

	case MODEM_STEP_GET_DEV_EUI:
		if (mFlag.gotDevEui) {
			OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT);
			goto_waitJoin();
		}
		break;

	case MODEM_STEP_WAIT_JOIN:
		if (modem.joined) {
			OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT);
			goto_sendAndWaitAck();
		}
		break;

	case MODEM_STEP_WAIT_ACK_DATA:
		if (mFlag.acked) {
			OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT);
			display_rssiSnr();
			MODEM_normalTerminaltion();
			// goto_getTime();
		} else if (mFlag.timeoutOccurred) {
			OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT);
			handle_timeout();
		}
		break;

		// case MODEM_STEP_GET_TIME:
		//     if (mFlag.gotTime) {
		//         MODEM_normalTerminaltion();
		//     }
		//     break;

	default:
		OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT);
		OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_TIMEOUT);
		break;
	}
}

void MODEM_handler()
{
	if (AppProcess != APP_IDLE) {
		MODEM_process();
	} else {
		OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_POWER_OFF);
	}
}

void MODEM_copyAppEui(uchar *aeui)
{
	memcpy(aeui, modem.appEui, 8);
}
void MODEM_copyDevEui(uchar *deui)
{
	memcpy(deui, modem.devEui, 8);
}
int MODEM_getLastRssi()
{
	return modem.lastRSSI;
}
int MODEM_getLastSNR()
{
	return modem.lastSNR;
}

int MODEM_inReportProcess()
{
	if (modem.step != MODEM_STEP_IDLE || AppProcess != APP_IDLE) {
		return 1;
	}
	return 0;
}

int MODEM_getAccessState()
{
	if (MODEM_inReportProcess()) {
		return ACCESS_STATE_IN_PROGRESS;
	}

#if 1
	return ACCESS_STATE_IDLE;
#else
	// SKT 정책에 따라 10초 내의 재송신을 방지하기 위한 코드임
	// 마지막 access가 종료된 때부터 7초를 기다려 재 access 절차를 시작하여야 함.
	Date_t now;
	RTC_read(&now);

	int32 secDiff = RTC_calcSecDiff(&modem.lastAccessTime, &now);

	if (secDiff < 0 || secDiff > 7) { // 10 sec
		return ACCESS_STATE_IDLE;
	}

	return ACCESS_STATE_WAIT_FEW_SEC;
#endif
}

int MODEM_getStateOrResult()
{
	if (modem.step == MODEM_STEP_IDLE) {
		return modem.lastError;
	} else {
		int status = DEVICE_STATUS_IDLE;
		switch (modem.step) {
		case MODEM_STEP_IDLE:
			status = DEVICE_STATUS_IDLE;
			break;
		case MODEM_STEP_CHECK_ALIVE:
		case MODEM_STEP_GET_APP_EUI:
		case MODEM_STEP_GET_DEV_EUI:
			status = DEVICE_STATUS_CHECK_MODEM;
			break;
		case MODEM_STEP_WAIT_JOIN:
			status = DEVICE_STATUS_WAIT_JOIN;
			break;
		case MODEM_STEP_WAIT_ACK_DATA:
		case MODEM_STEP_GET_RSSI:
		case MODEM_STEP_GET_SNR:
			status = DEVICE_STATUS_COMM;
			break;
		case MODEM_STEP_GET_TIME:
			status = DEVICE_STATUS_GET_TIME;
			break;
		}
		return status;
	}
}

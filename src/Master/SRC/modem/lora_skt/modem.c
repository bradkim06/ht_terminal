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
#include "assert.h"
#include "loraModem.h"

extern Config_t conf;
extern uint8 AppProcess;

Modem_t modem;

static BOOL isPowerOn = FALSE;

void MODEM_turnOff()
{
	if (isPowerOn) {
		printf_ts("MODEM : power off\n");
		// RF_PWR_CON(P2.0)
		P2SEL &= ~0x01;
		P2DIR |= 0x01;
		P2OUT |= 0x01;

		P2SEL &= ~0x08;
		P2DIR |= 0x08;
		P2OUT &= ~0x08;

		isPowerOn = FALSE;
		modemCtx.status.provision = 0;
		memset(&modemCtx.joinTime, 0, sizeof(Date_t));
	}
}

void MODEM_turnOn()
{
	if (!isPowerOn) {
		printf_ts("MODEM : power on\n");
		// RF_PWR_CON(P2.0)
		P2SEL &= ~0x01;
		P2DIR |= 0x01;
		P2OUT &= ~0x01;

		P2SEL &= ~0x08;
		P2DIR |= 0x08;
		P2OUT |= 0x08;

		isPowerOn = TRUE;
	}
}

void MODEM_reset()
{
	// reset LoRa Modem: NRESET(P2.3)
	P2SEL &= ~0x08;
	P2DIR |= 0x08;

	P2OUT &= ~0x08;
	MISC_delayMs(500);
	P2OUT |= 0x08;

	modemCtx.status.provision = 0;
	memset(&modemCtx.joinTime, 0, sizeof(Date_t));
}

void MODEM_write(char *pCmd)
{
	if (TEST_isTestMode() == FALSE) {
		printf(TP_ANSI_FG_BLUE);
		printf_ts("%s\n", pCmd);
		printf(TP_ANSI_RESET);
	}

	int len = strlen(pCmd);
	UART_send(UART_A2, pCmd, len, TRUE);
}

void MODEM_initialization()
{
	memset(&modem, 0, sizeof(Modem_t));
	memset(&modemCtx, 0, sizeof(ModemContext_t));
	memset(&modemComm, 0, sizeof(ModemComm_t));
	modemCtx.step = MODEM_STEP_IDLE;
	modemCtx.status.stop = 1;
	// PIN 초기화 시 VCC on/off 여부 확인.
	isPowerOn = (P2OUT & BM(0)) ? FALSE : TRUE;
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

	if (modemCtx.step != MODEM_STEP_IDLE) {
		return;
	} else {
		// 모뎀 동작을 위해 stop/busy flag만 초기화.
		modemCtx.status.stop = 0;
		modemCtx.status.busy = 0;
	}

	// 단말기 부팅 후 최초 모뎀 동작 시 Power on 상태인 경우 모뎀 리셋
	// 모뎀 리셋이 없는 경우 단말기 초기화 과정에서 JOIN이 완료되어
	// 동작과정에서 JOIN완료 여부를 확인 할 수 없어 리셋 과정을 추가.
	if (AppProcess == APP_INITIAL_REPORT && isPowerOn) {
		MODEM_reset();
	}

	MODEM_enable();

	MODEM_turnOn();
	if (AppProcess == APP_IMMEDIATE_REPORT || AppProcess == APP_INITIAL_REPORT) {
		// MODEM VCC off->on 시 LCD가 reset되는 문제가 있어 이에 대한 workaround delay 추가.
		// 필요 delay는 최소 250ms 이므로 H/W편차를 고려하여 500ms delay를 설정.
		MISC_delayMs(500);
		LCD_accessModem();
	}

	OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
}

void MODEM_stop(int resultCode)
{
	// 모뎀동작 완료 시 stop flag set, busy flag clear.
	modemCtx.status.stop = 1;
	modemCtx.status.busy = 0;
	modemCtx.step = MODEM_STEP_IDLE;
	if (resultCode == SUCCESS) {
		modemCtx.errCode = MODEM_ERROR_NONE;
	}
	RTC_read(&modemCtx.closeTime);

	if (AppProcess == APP_IMMEDIATE_REPORT || AppProcess == APP_INITIAL_REPORT) {
		char buf[0x10] = "";

		if (modemCtx.errCode == MODEM_ERROR_NONE) {
			snprintf(buf, 0x10, "SUCCESS");
		} else {
			snprintf(buf, 0x10, "FAIL %d", MODEM_getStateOrResult());
		}
		LCD_displayString(buf);

		MISC_delayMs(1000);
		LCD_redraw();
	}

	if (modemCtx.errCode == MODEM_ERROR_NONE ||
	    modemCtx.errCode == MODEM_ERROR_TIME_SYNC_FAIL) {
		// 시간동기화 실패는 데이터 통신에는 성공한 것이므로 실패가 아닌 것으로 처리.
		modemCtx.errorCnt = 0;
	} else {
		modemCtx.errorCnt++;
	}
	printf_ts("MODEM : working stop - %s\n",
		  (modemCtx.errCode == MODEM_ERROR_NONE) ? "SUCCESS" : "FAIL");
#if DEBUG
	printf("  ERROR(%d) BUSY(%d) STOP(%d) JOIN(%d) TIMESYNC(%d) \n", modemCtx.errCode,
	       modemCtx.status.busy, modemCtx.status.stop, modemCtx.status.provision,
	       modemCtx.status.timeSync);
	printf("  CLOSE TIME : %04d-%02d-%02d %02d:%02d:%02d \n", modemCtx.closeTime.year,
	       modemCtx.closeTime.mon, modemCtx.closeTime.day, modemCtx.closeTime.hour,
	       modemCtx.closeTime.min, modemCtx.closeTime.sec);
	printf("  JOIN  TIME : %04d-%02d-%02d %02d:%02d:%02d \n", modemCtx.joinTime.year,
	       modemCtx.joinTime.mon, modemCtx.joinTime.day, modemCtx.joinTime.hour,
	       modemCtx.joinTime.min, modemCtx.joinTime.sec);
#endif
	OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_POWER_OFF);
}

void MODEM_close()
{
	MODEM_disable();

	// NFC sleep 인 경우 VCC off
	if (conf.sleepMode) {
		MODEM_turnOff();
	}

	// 2회 연속 실패 혹은 Join 실패인 경우 VCC off
	if (modemCtx.errorCnt >= MODEM_MAX_TRY_COUNT || !modemCtx.status.provision) {
		MODEM_turnOff();
	}

	OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT);
	OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_RX);
	OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_POWER_OFF);

	// TODO : will check and remove step initialize
	modemCtx.step = MODEM_STEP_IDLE;
	AppProcess = APP_IDLE;
}

void MODEM_timeout()
{
	OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT);

	if (modemComm.retry >= 1) {
		modemComm.retry--;
		MODEM_write(modemComm.atData);
		OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT, modemComm.timeout);
	} else {
		MODEM_stop(FAIL);
	}
}

void MODEM_handler()
{
	MODEM_process(AppProcess);
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

	MODEM_response(buf, len);

	OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
}

BOOL MODEM_setUserNwk(uchar *aeui, uchar *akey, JoinMode_t joinMode)
{
	if (joinMode == modem.lastUserJoin) { // 모든 요청값이 현재값과 동일할 경우 return FALSE.
		if (memcmp(aeui, modem.appEui, 8) == 0 && memcmp(akey, modem.appKey, 16) == 0) {
			return FALSE;
		}
	}

	if (modemCtx.step != MODEM_STEP_IDLE) {
		// 동작 중인 경우 Context 초기화 및 VCC off
		MODEM_turnOff();
		AppProcess = APP_IDLE;
		MODEM_initialization();
	}
	memcpy(modem.userAppKey, akey, 16);
	memcpy(modem.userAppEui, aeui, 8);
	modem.lastUserJoin = joinMode;
	MODEM_open(APP_SET_CONFIGURATION);

	return TRUE;
}

void MODEM_copyAppKey(uchar *akey)
{
	memcpy(akey, modem.appKey, 16);
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
	return modem.sigQuality.rssi;
}
int MODEM_getLastSNR()
{
	return modem.sigQuality.snr;
}

int MODEM_getAccessState()
{
	if (modemCtx.step != MODEM_STEP_IDLE || AppProcess != APP_IDLE) {
		if (AppProcess == APP_SET_CONFIGURATION)
			return ACCESS_STATE_IN_SETTING;
		else {
			return ACCESS_STATE_IN_PROGRESS;
		}
	}

	// SKT 정책에 따라 10초 내의 재송신을 방지하기 위한 코드임
	// 마지막 access가 종료된 때부터 7초를 기다려 재 access 절차를 시작하여야 함.
	Date_t now;
	RTC_read(&now);

	int32 secDiff = RTC_calcSecDiff(&modemCtx.closeTime, &now);

	if (secDiff < 0 || secDiff > 7) { // 10 sec
		return ACCESS_STATE_IDLE;
	}

	return ACCESS_STATE_WAIT_FEW_SEC;
}

int MODEM_getStateOrResult()
{
	if (modemCtx.step == MODEM_STEP_IDLE) {
		int error = ERROR_CODE_UNKNOWN;
		switch (modemCtx.errCode) {
		case MODEM_ERROR_NONE:
			error = ERROR_CODE_NONE;
			break;
		case MODEM_ERROR_NO_RESP:
		case MODEM_ERROR_NOT_ACTIVE:
			error = ERROR_CODE_MODEM;
			break;
		case MODEM_ERROR_JOIN_FAIL:
			error = ERROR_CODE_NOT_JOINED;
			break;
		case MODEM_ERROR_DATA_FAIL:
			error = ERROR_CODE_COMM;
			break;
		case MODEM_ERROR_TIME_SYNC_FAIL:
			error = ERROR_CODE_GET_TIME;
			break;
		default:
			error = ERROR_CODE_UNKNOWN;
			break;
		}
		return error;
	} else {
		int status = DEVICE_STATUS_IDLE;
		switch (modemCtx.step) {
		case MODEM_STEP_IDLE:
			status = DEVICE_STATUS_IDLE;
			break;
		case MODEM_STEP_CHECK_ALIVE:
		case MODEM_STEP_GET_NWK_SESSION:
		case MODEM_STEP_SET_NWK_SESSION:
			status = DEVICE_STATUS_CHECK_MODEM;
			break;
		case MODEM_STEP_WAIT_PROVISION:
			status = DEVICE_STATUS_WAIT_JOIN;
			break;
		case MODEM_STEP_WAIT_ACK_DATA:
		case MODEM_STEP_GET_SIG:
			status = DEVICE_STATUS_COMM;
			break;
		case MODEM_STEP_GET_TIME:
			status = DEVICE_STATUS_GET_TIME;
			break;
		}
		return status;
	}
}

void MODEM_copyLastAccessTime(uint8 *p)
{
	*(p + 0) = modemCtx.closeTime.year % 0x100;
	*(p + 1) = modemCtx.closeTime.year / 0x100;
	*(p + 2) = modemCtx.closeTime.mon;
	*(p + 3) = modemCtx.closeTime.day;
	*(p + 4) = modemCtx.closeTime.hour;
	*(p + 5) = modemCtx.closeTime.min;
	*(p + 6) = modemCtx.closeTime.sec;
}

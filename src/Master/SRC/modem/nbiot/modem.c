#include <msp430.h>
#include <ctype.h>
#include <time.h>

#include "common_header.h"
#include "port_desc.h"
#include "uart.h"
#include "check_meter_misc.h"

#include "flashDriver.h"
#include "osal_Timer.h"
#include "app.h"
#include "meter.h"
#include "lcdDriver.h"
#include "shell.h"
#include "rtcAlarm.h"
#include "modem.h"
#include "message.h"
#include "nbiotModem.h"
#include "assert.h"
#include "battery.h"
#include "test.h"

extern Config_t conf;
extern uint8 AppProcess;

Modem_t modem;

/**
 * @brief MODEM init flag print macro (only debug)
 */
static void MODEM_STATUS_PRINT()
{
#if defined(DEBUG)
	printf("=============================================================\n");
	printf("[ FLAG         ] STATUS(%02X) PROC(%02X) LWM2M(%02X) \n", modemCtx.status.value,
	       modemCtx.proc.value, modemCtx.lwm2m.value);
	printf("[ USIM TYPE    ] %s\n", (modem.modemType == MODEM_TYPE_UPLUS) ? "UPLUS" : "KT");
	printf("[ OPERATION    ] STEP(%s) ERROR(%02X) RETRY(%s/%d)\n",
	       MODEM_STEP_STRING(modemCtx.step), modemCtx.errCode,
	       MODEM_STEP_STRING(modemCtx.retryStep), modemCtx.retryCount);
	printf("[ UDP UL/DL    ] %d/%d\n", modemCtx.ulCnt, modemCtx.dlCnt);
	printf("[ PF  UL/DL    ] %d/%d\n", modemCtx.pfUlCnt, modemCtx.pfDlCnt);
	printf("[ LAST UPDATE  ] %04d-%02d-%02d %02d:%02d:%02d\n", modem.lastUpdateTime.year,
	       modem.lastUpdateTime.mon, modem.lastUpdateTime.day, modem.lastUpdateTime.hour,
	       modem.lastUpdateTime.min, modem.lastUpdateTime.sec);
	printf("[ LAST CERTIFY ] %04d-%02d-%02d %02d:%02d:%02d\n", modem.lastCertifyTime.year,
	       modem.lastCertifyTime.mon, modem.lastCertifyTime.day, modem.lastCertifyTime.hour,
	       modem.lastCertifyTime.min, modem.lastCertifyTime.sec);
	printf("[ LAST ACCESS  ] %04d-%02d-%02d %02d:%02d:%02d\n", modem.lastAccessTime.year,
	       modem.lastAccessTime.mon, modem.lastAccessTime.day, modem.lastAccessTime.hour,
	       modem.lastAccessTime.min, modem.lastAccessTime.sec);
	for (int i = 0; i < CNT_ERROR_LOG; i++) {
		printf("[ LOG #%02d      ] 20%02X-%02X-%02X %02X:%02X:%02X ERR(%02X)\n", i,
		       modemCtx.errLog[i][0], modemCtx.errLog[i][1], modemCtx.errLog[i][2],
		       modemCtx.errLog[i][3], modemCtx.errLog[i][4], modemCtx.errLog[i][5],
		       modemCtx.errLog[i][6]);
	}
	printf("=============================================================\n");
#endif
}

static BOOL isPowerOn = FALSE;

void MODEM_turnOff()
{
	if (isPowerOn) {
		printf_ts("MODEM : power off\n");

#if (DEVICE_REVISION == DEV_REV_PWRCTRL_MOSFET_LDO)
		if (conf.bslModel) {
			// MOSFET work turn on power (P4.6)
			P4SEL &= ~BM(6);
			P4DIR |= BM(6);
			P4OUT |= BM(6); // make low
		} else {
			// MOSFET work turn on power (P1.1)
			P1SEL &= ~BM(1);
			P1DIR |= BM(1);
			P1OUT |= BM(1); // make low
		}

		MISC_delayMs(10);

		// LDO work turn off power (P2.0)
		P2SEL &= ~BM(0);
		P2DIR |= BM(0);
		P2OUT &= ~BM(0); // make low

		MISC_delayMs(100);
#elif (DEVICE_REVISION == DEV_REV_PWRCTRL_LDO_ONLY)
		// LDO work turn off power (P2.0)
		P2SEL &= ~BM(0);
		P2DIR |= BM(0);
		P2OUT &= ~BM(0); // make low

		MISC_delayMs(100);
#elif (DEVICE_REVISION == DEV_REV_PWRCTRL_MOSFET_ONLY)
		// turn off power (P2.0)
		P2SEL &= ~BM(0);
		P2DIR |= BM(0);
		P2OUT |= BM(0); // make high

		MISC_delayMs(10);
#endif

		isPowerOn = FALSE;

		modemCtx.lwm2m.value = 0;
		modemCtx.status.cellreg = 0;
		memset(&modem.lastCertifyTime, 0, sizeof(Date_t));
	}
}

void MODEM_turnOn()
{
	if (!isPowerOn) {
		printf_ts("MODEM : power on\n");

#if (DEVICE_REVISION == DEV_REV_PWRCTRL_MOSFET_LDO)
		// LDO work turn on power (P2.0)
		P2SEL &= ~BM(0);
		P2DIR |= BM(0);
		P2OUT |= BM(0); // make high

		MISC_delayMs(100);

		if (conf.bslModel) {
			// MOSFET work turn on power (P4.6)
			P4SEL &= ~BM(6);
			P4DIR |= BM(6);
			P4OUT &= ~BM(6); // make low
		} else {
			// MOSFET work turn on power (P1.1)
			P1SEL &= ~BM(1);
			P1DIR |= BM(1);
			P1OUT &= ~BM(1); // make low
		}

		MISC_delayMs(10);
#elif (DEVICE_REVISION == DEV_REV_PWRCTRL_LDO_ONLY)
		// LDO work turn on power (P2.0)
		P2SEL &= ~BM(0);
		P2DIR |= BM(0);
		P2OUT |= BM(0); // make high

		MISC_delayMs(100);
#elif (DEVICE_REVISION == DEV_REV_PWRCTRL_MOSFET_ONLY)
		// turn on power (P2.0)
		P2SEL &= ~BM(0);
		P2DIR |= BM(0);
		P2OUT &= ~BM(0); // make low

		MISC_delayMs(10);
#endif
		isPowerOn = TRUE;
	}
}

void MODEM_sleep()
{
#if (DEVICE_REVISION == DEV_REV_PWRCTRL_MOSFET_LDO)
	printf_ts("MODEM : sleep");

	if (!isPowerOn) {
		printf(" - fail, MODEM is not on\n");
	} else {
		printf(" - success\n");

		// LDO work
		P2SEL &= ~BM(0);
		P2DIR |= BM(0);
		P2OUT &= ~BM(0); // make low

		MISC_delayMs(100);
	}
#endif
}

void MODEM_wakeup()
{
#if (DEVICE_REVISION == DEV_REV_PWRCTRL_MOSFET_LDO)
	printf_ts("MODEM : wake-up");

	if (!isPowerOn) {
		printf(" - fail, MODEM is not on\n");
	} else {
		printf(" - success\n");

		// LDO work
		P2SEL &= ~BM(0);
		P2DIR |= BM(0);
		P2OUT |= BM(0); // make high

		MISC_delayMs(100);
	}
#endif
}

void MODEM_initialization()
{
	memset(&modem, 0, sizeof(modem));
	memset(&modemCtx, 0, sizeof(modemCtx));
	memset(&modemComm, 0, sizeof(modemComm));
}

void MODEM_enable()
{
	UartInitParam_t param = { .isTxEnable = TRUE,
				  .isRxEnable = TRUE,
				  .baudrate = UART_BAUDRATE_9600,
				  .parity = UART_PARITY_NONE,
				  .stopbit = UART_STOPBIT_ONE,
				  .charLen = UART_8BITS_CHAR };

	UART_open(UART_A2, &(param));
}

void MODEM_disable()
{
	if (modemCtx.status.psmOn) {
		UART_close(UART_A2, GPIO_INPUT);
	} else {
		UART_close(UART_A2, GPIO_LOW);
	}
}

void MODEM_write(char *pCmd)
{
	if (TEST_isTestMode() == FALSE) {
		printf(TP_ANSI_FG_BLUE);
		// AT CMD가 512 byte에 근접할 경우 buffer overflow의 위험이 있어 timestamp 출력을 분리
		printf_ts("");
		printf("%s\n", pCmd);
		printf(TP_ANSI_RESET);
	}

	int len = strlen(pCmd);
	UART_send(UART_A2, pCmd, len, TRUE);
}

void MODEM_read()
{
	char buf[MODEM_RX_BUF_LEN + 1]; // 1 for margin
	memset(buf, 0, MODEM_RX_BUF_LEN + 1);
	int len = UART_receive(UART_A2, buf, MODEM_RX_BUF_LEN);

	if (TEST_isTestMode() == FALSE && len > 0) {
		printf(TP_ANSI_FG_RED);
		PRINT_string(buf, len);
		printf(TP_ANSI_RESET);
	}

	// Stop 상태인 경우 MODEM response 무시.
	if (modemCtx.status.stop) {
		return;
	}

	MODEM_response(buf, len);
	OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
}

static void saveModemErrorLog(Date_t *date, ModemErrorCode_t errCode)
{
	/**
     * @brief error log 상에서 시간비교를 통해 가장 오래된 log에 overwrite.
     *        errLog 배열의 index 1 ~ 6은 날짜 BCD 값이므로 따로 변환 없이 비교.
     */
	int old = 0;
	for (int i = 1; i < CNT_ERROR_LOG; i++) {
		// year, month, day value mix and compare
		uint32 val1 = (uint32)modemCtx.errLog[old][0] << 16 | modemCtx.errLog[old][1] << 8 |
			      modemCtx.errLog[old][2];
		uint32 val2 = (uint32)modemCtx.errLog[i][0] << 16 | modemCtx.errLog[i][1] << 8 |
			      modemCtx.errLog[i][2];
		if (val1 > val2) {
			old = i;
		} else if (val1 == val2) {
			// hour, min, sec value mix and compare
			val1 = (uint32)modemCtx.errLog[old][3] << 16 |
			       modemCtx.errLog[old][4] << 8 | modemCtx.errLog[old][5];
			val2 = (uint32)modemCtx.errLog[i][3] << 16 | modemCtx.errLog[i][4] << 8 |
			       modemCtx.errLog[i][5];
			if (val1 > val2) {
				old = i;
			}
		}
	}

	uint32 temp = 0;
	modemCtx.errLog[old][6] = errCode;

	temp = date->year - 2000;
	int2bcd(&temp, &modemCtx.errLog[old][0], 1);

	temp = date->mon;
	int2bcd(&temp, &modemCtx.errLog[old][1], 1);

	temp = date->day;
	int2bcd(&temp, &modemCtx.errLog[old][2], 1);

	temp = date->hour;
	int2bcd(&temp, &modemCtx.errLog[old][3], 1);

	temp = date->min;
	int2bcd(&temp, &modemCtx.errLog[old][4], 1);

	temp = date->sec;
	int2bcd(&temp, &modemCtx.errLog[old][5], 1);
}

void MODEM_stop(int resultCode)
{
	OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_POWER_OFF);
	printf_ts("MODEM : stop working (%s) \n", (resultCode == SUCCESS) ? "SUCCESS" : "FAIL");

	if (resultCode == SUCCESS) {
		modemCtx.errCode = MODEM_ERROR_NONE;
#if defined(MODEM_FUN_EN_PSM)
		if (modemCtx.proc.runFota) {
			// FOTA 미완료 상태로 동작 완료된 경우 PSM off.
			modemCtx.status.psmOn = 0;
		} else {
			// 동작 완료 후 Detached 된 경우 PSM off.
			modemCtx.status.psmOn =
				(modemCtx.status.cellreg == MODEM_CELLREG_ATTACHED) ? 1 : 0;
		}
#else
		modemCtx.status.psmOn = 0;
#endif

		if (AppProcess == APP_PERIODIC_REPORT) {
			METER_clearStoredData();
		}

		if (AppProcess == APP_IMMEDIATE_REPORT || AppProcess == APP_INITIAL_REPORT) {
			char rssiSNR[0x10] = "";
			memset(rssiSNR, ' ', 0x10);
			sprintf(rssiSNR, "-%ddBm  %d", MODEM_getLastRsrp(), MODEM_getLastSNR());
			LCD_displayString(rssiSNR);
			MISC_delayMs(2000);
			LCD_redraw();
		}
	} else {
		// Fail인 경우 PSM disable
		modemCtx.status.psmOn = 0;
		// Error code가 AT command no resp이고 ERROR flag가 set된 경우 AT command fail로 변경.
		if (modemCtx.status.error) {
			modemCtx.errCode = (modemCtx.errCode == MODEM_ERROR_AT_CMD_NO_RESP) ?
						   MODEM_ERROR_AT_CMD_FAIL :
						   modemCtx.errCode;
		}

		// Fail인 경우 동작 중간에 종료하므로 process done flag를 set해야 함.
		if (AppProcess == APP_INITIAL_REPORT || AppProcess == APP_IMMEDIATE_REPORT) {
			LCD_displayError(ERROR_REPORT);
			MISC_delayMs(2000);
			LCD_redraw();
		}
	}

	// 동작 완료 후 관련 context clear
	modemCtx.retryCount = 0;
	modemCtx.retryStep = MODEM_STEP_UNKNOWN;
	modemCtx.step = MODEM_STEP_IDLE;
	modemCtx.stepReset = TRUE;

	modemCtx.status.error = 0;
	modemCtx.status.busy = 0;
	modemCtx.status.stop = 1;

	RTC_read(&modem.lastAccessTime);
	if (modemCtx.errCode != MODEM_ERROR_NONE) {
		saveModemErrorLog(&modem.lastAccessTime, modemCtx.errCode);
	}
	MODEM_STATUS_PRINT();

	OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_POWER_OFF);
}

void MODEM_timeout()
{
	OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT);

	if (modemComm.retry >= 1) {
		modemComm.retry--;
		MODEM_write(modemComm.atData);

		OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT, modemComm.timeout);
	} else {
		printf_ts("MODEM : state(%s) retry state(%s) retry count(%d)",
			  MODEM_STEP_STRING(modemCtx.step), MODEM_STEP_STRING(modemCtx.retryStep),
			  modemCtx.retryCount);

		// Clear MODEM AT command working flag
		modemCtx.status.busy = 0;

		// INIT/RETRY 단계가 아니면서 현재 재시도 횟수가 MAX미만인 경우 재시도.
		// 그 외 INIT/RETRY 단계이거나 현재 재시도 횟수가 MAX이상인 경우 종료.
		static int maxRetryCount = 0;
		if (modemCtx.retryStep == MODEM_STEP_UNKNOWN) {
			switch (modemCtx.step) {
			case MODEM_STEP_BIP:
				maxRetryCount = NBIOT_BIP_RETRY;
				break;
			case MODEM_STEP_ATTACH_NW:
				maxRetryCount = NBIOT_ATTACH_RETRY;
				break;
			case MODEM_STEP_CERTIFY:
				maxRetryCount = NBIOT_ONEM2M_RETRY;
				break;
			default:
				maxRetryCount = 0;
				break;
			}
		}

		// 재시도 과정에서 실패할 경우 재시도 횟수와 관계없이 Fail처리.
		if (modemCtx.step != MODEM_STEP_RETRY && modemCtx.retryCount < maxRetryCount) {
			printf("- retry(%d/%d)\n", modemCtx.retryCount, maxRetryCount);

			modemCtx.retryCount++;
			modemCtx.retryStep = modemCtx.step;
			modemCtx.step = MODEM_STEP_RETRY;
			modemCtx.stepReset = TRUE;

			// Retry count 증가 후 마지막 재시도로 판단될 경우 H/W reset on
			// 단, Max retry 가 단 1회인 경우 H/W reset 을 사용하지 않는다.
			if (maxRetryCount == 1) {
				modemCtx.proc.hwRstRetry = FALSE;
			} else {
				modemCtx.proc.hwRstRetry =
					(modemCtx.retryCount == maxRetryCount) ? TRUE : FALSE;
			}
			OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
		} else {
			printf("- fail and close(%d/%d)\n", modemCtx.retryCount, maxRetryCount);
			MODEM_stop(FAIL);
		}
	}
}

void MODEM_open(int process)
{
	if (modemCtx.step != MODEM_STEP_IDLE) {
		return;
	} else {
		// Process 시작 전 status flag 초기화
		modemCtx.status.error = 0;
		modemCtx.status.busy = 0;
		modemCtx.status.stop = 0;
		modemCtx.status.lwm2mOn = 0;
	}

	AppProcess = process;
	if (AppProcess == APP_IMMEDIATE_REPORT || AppProcess == APP_INITIAL_REPORT) {
		LCD_accessModem();
	}

	MODEM_enable();
	if (modemCtx.status.psmOn) {
		MODEM_wakeup();
	} else {
		MODEM_turnOn();
	}

	OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
}

void MODEM_close()
{
	// 단말기 sleep mode 인 경우 sleep disable.
	if (conf.sleepMode) {
		modemCtx.status.psmOn = 0;
	}

	// MODEM 동작 중간에 발생할 경우 IDLE로 설정하고 sleep disable.
	if (modemCtx.step != MODEM_STEP_IDLE) {
		modemCtx.step = MODEM_STEP_IDLE;
		modemCtx.status.psmOn = 0;
	}
	modemCtx.status.stop = 1;

	MODEM_disable();
	if (modemCtx.status.psmOn) {
		MODEM_sleep();
	} else {
		MODEM_turnOff();
	}

	if (AppProcess == APP_IMMEDIATE_REPORT || AppProcess == APP_INITIAL_REPORT) {
		char buf[0x10] = "";

		if (modemCtx.errCode == ERROR_CODE_NONE) {
			snprintf(buf, 0x10, "SUCCESS");
		} else {
			snprintf(buf, 0x10, "FAIL %d", modemCtx.errCode);
		}
		LCD_displayString(buf);

		MISC_delayMs(1000);
		LCD_redraw();
	}

	OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT);
	OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_RX);
	OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_POWER_OFF);

	AppProcess = APP_IDLE;
}

void MODEM_handler()
{
	if (modemCtx.status.busy || modemCtx.status.stop) {
		return;
	}

	MODEM_process();
}

static void str2BcdForImeiAndImsi(uchar *pBCD, char *pString)
{
	for (int i = 0; i < 7; i++) {
		*(pBCD + i) = ascii2BCD(*(pString + i * 2 + 0), *(pString + i * 2 + 1));
	}

	*(pBCD + 7) = ascii2BCD(*(pString + 14), 'F');
}

void MODEM_copyImei(uchar *imei)
{
	str2BcdForImeiAndImsi(imei, modem.imeiStr);
}
void MODEM_copyImsi(uchar *imsi)
{
	str2BcdForImeiAndImsi(imsi, modem.imsiStr);
}
void MODEM_copyCtn(uchar *ctn)
{
	// 11자리의 CTN을 BCD로 변환 시 가장 앞자리 0을 1byte로 할당한다.
	// EX :  012-1234-5678 -> 0x00 0x12 0x12 0x34 0x56 0x78
	char *p = modem.ctnStr;
	int len = sizeof(modem.ctnStr) / 2;
	*(ctn + 0) = ascii2BCD('0', '0');
	for (int i = 1; i < len; i++) {
		*(ctn + i) = ascii2BCD(*(p + i * 2 - 1), *(p + i * 2));
	}
}

void MODEM_copyIccid(uchar *iccid)
{
	char *p = modem.iccidStr;
	int len = sizeof(modem.iccidStr) / 2;
	for (int i = 0; i < len; i++) {
		*(iccid + i) = ascii2BCD(*(p + i * 2), *(p + i * 2 + 1));
	}
}

void MODEM_copyCseId(uchar *cseid)
{
	memcpy(cseid, modem.epName, sizeof(modem.epName));
}

int MODEM_getLastRssi()
{
	return (abs(modem.modemQuality.lastRSSI) / 10);
}

int MODEM_getLastRsrp()
{
	return (abs(modem.modemQuality.rsrp) / 10);
}

int MODEM_getLastRsrq()
{
	return (abs(modem.modemQuality.rsrq) / 10);
}

int MODEM_getLastSNR()
{
	return (modem.modemQuality.snr / 10);
}

int MODEM_getAccessState()
{
	if (modemCtx.step != MODEM_STEP_IDLE || AppProcess != APP_IDLE) {
		printf_ts("MODEM : working, state(%d) process(%d) \n", modemCtx.step, AppProcess);
		return ACCESS_STATE_IN_PROGRESS;
	}

	Date_t now;
	RTC_read(&now);

	int32 secDiff = RTC_calcSecDiff(&modem.lastAccessTime, &now);

	if (secDiff < 0 || secDiff > 3) { // 모뎀 동작에 대해 Interval을 위해 추가.
		return ACCESS_STATE_IDLE;
	}

	return ACCESS_STATE_WAIT_FEW_SEC;
}

int MODEM_getStateOrResult()
{
	if (modemCtx.status.stop && modemCtx.step == MODEM_STEP_IDLE) {
		int error = ERROR_CODE_UNKNOWN;
		switch (modemCtx.errCode) {
		case MODEM_ERROR_NONE:
		case MODEM_ERROR_PF_EPNS_CHANGE: // EPNS 변경 발생. (error는 아님.)
		case MODEM_ERROR_PF_FOTA_FAIL: // Platform module FOTA 실패
			error = ERROR_CODE_NONE;
			break;

		case MODEM_ERROR_AT_CMD_NO_RESP: // AT command no response
		case MODEM_ERROR_AT_CMD_FAIL: // AT command error
			error = ERROR_CODE_MODEM;
			break;

		case MODEM_ERROR_USIM_INVALID: // AT+CIMI 무응답 OR ERROR
			error = ERROR_CODE_SIM;
			break;

		case MODEM_ERROR_PF_UL_FAIL: // Platform 연동 시 UL 실패
		case MODEM_ERROR_PF_UD_FAIL: // Platform 연동 시 DL 실패
		case MODEM_ERROR_UDP_UL_FAIL: // UDP UL 실패
		case MODEM_ERROR_UDP_DL_FAIL: // UDP DL 실패
			error = ERROR_CODE_COMM;
			break;

		case MODEM_ERROR_PF_CERITY_FAIL: // Bootstrap or Register 실패
			error = ERROR_CODE_FAIL_CERTIFY;
			break;

		case MODEM_ERROR_ATTACH_FAIL: // 망접속 실패
			error = ERROR_CODE_FAIL_ATTACH;
			break;
		}

		return error;
	} else {
		int status = DEVICE_STATUS_UNKNOWN;
		switch (modemCtx.step) {
		case MODEM_STEP_IDLE:
			status = DEVICE_STATUS_IDLE;
			break;

		case MODEM_STEP_INIT:
		case MODEM_STEP_BIP:
			status = DEVICE_STATUS_CHECK_MODEM;
			break;

		case MODEM_STEP_RETRY:
		case MODEM_STEP_ATTACH_NW:
		case MODEM_STEP_DETACH_NW:
		case MODEM_STEP_UPDATE_QA:
		case MODEM_STEP_CERTIFY:
		case MODEM_STEP_FOTA:
		case MODEM_STEP_TRANSFER:
			status = DEVICE_STATUS_COMM;
			break;
		}

		return status;
	}
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

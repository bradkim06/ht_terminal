#include <msp430.h>
#include <ctype.h>
#include <time.h>

#include "common_header.h"
#include "uart.h"
#include "check_meter_misc.h"
#include "osal_Timer.h"
#include "app.h"
#include "rtcAlarm.h"
#include "lcdDriver.h"
#include "assert.h"

#include "modem.h"
#include "message.h"
#include "loraModem.h"

#define MODEM_BOOTUP_DELAY_MS 1000
#define MODEM_JOIN_TIMEOUT_SEC 3600 // 1 hour

#define MODEM_ATCMD_TIMEOUT_MS 3000
#define MODEM_ATCMD_RETRY_CNT 3

// Timeout = Tx(3sec) + Rx(6sec) + extra(9), Extra is a time value considering LBT
#define MODEM_DATA_TIMEOUT_MS 18000
#define MODEM_DATA_RETRY_CNT 0

const AtCmd_t AtCmdCheckAlive = { ATCMD_IDX_CHECK_ALIVE, "AT+FWI" };
const AtCmd_t AtCmdSetDr = { ATCMD_IDX_SET_DR, "AT+DR " }; // Set data rate
const AtCmd_t AtCmdSetAdr = { ATCMD_IDX_SET_ADR, "AT+ADR " }; // Set ADR
const AtCmd_t AtCmdSendData = { ATCMD_IDX_SEND_DATA, "AT+SEND 02" }; // Application Port No 02
const AtCmd_t AtCmdGetDevEui = { ATCMD_IDX_GET_DEVEUI, "AT+DEUI" };
const AtCmd_t AtCmdGetAppEui = { ATCMD_IDX_GET_APPEUI, "AT+AEUI" };
const AtCmd_t AtCmdGetAppKey = { ATCMD_IDX_GET_APPKEY, "AT+AK" };
const AtCmd_t AtCmdSetAppEui = { ATCMD_IDX_SET_APPEUI, "AT+AEUI " };
const AtCmd_t AtCmdSetAppKey = { ATCMD_IDX_SET_APPKEY, "AT+AK " };
const AtCmd_t AtCmdGetSig = { ATCMD_IDX_GET_SIG, "AT+SIG" };
const AtCmd_t AtCmdGetTime = { ATCMD_IDX_GET_TIME, "AT+DEVT" };
const AtCmd_t AtCmdSaveCfg = { ATCMD_IDX_SAVE_CFG, "AT+SCFG" };
const AtCmd_t AtCmdResetNwk = { ATCMD_IDX_RESET_NWK, "AT+PS " };

extern Config_t conf;
extern uint8 AppProcess;

ModemContext_t modemCtx;
ModemComm_t modemComm;

/**
 * @brief AT command send wrapper function
 *
 * @param timeout timeout(ms) of each AT command
 * @param retry   AT command retry count
 * @param cmd     AT command (string)
 */
static void sendAtCommand(uint32 timeout, int retry, const AtCmd_t *atCmd, char *param)
{
	// if (retry >= 0 && !modemCtx.status.busy) {
	if (retry >= 0) {
		// 마지막 AT command와 동일하면서 Retry 횟수가 남아있는 경우 전송하지 않는다.
		if (modemComm.lastAtCmd == atCmd && modemComm.retry < 0) {
			return;
		}

		modemComm.lastAtCmd = atCmd;
		if (param != NULL) {
			snprintf(modemComm.atData, MODEM_MAX_AT_CMD_LEN, "%s%s", atCmd->cmd, param);
		} else {
			snprintf(modemComm.atData, MODEM_MAX_AT_CMD_LEN, "%s", atCmd->cmd);
		}

		modemComm.retry = retry;
		modemComm.timeout = timeout;
		modemCtx.status.busy = 1;

		MODEM_write(modemComm.atData);
		OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT, modemComm.timeout);
	}
}

static BOOL idle()
{
	modemCtx.errCode = MODEM_ERROR_NO_RESP;
	MISC_delayMs(MODEM_BOOTUP_DELAY_MS);
	OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
	return TRUE;
}

static BOOL checkAlive()
{
	modemCtx.errCode = MODEM_ERROR_NOT_ACTIVE;
	if (modemCtx.status.busy) {
		return FALSE;
	}

	if (!modemCtx.status.alive) {
		sendAtCommand(MODEM_ATCMD_TIMEOUT_MS, MODEM_ATCMD_RETRY_CNT, &AtCmdCheckAlive,
			      NULL);
		return FALSE;
	}

	if (!modemCtx.status.provision) {
		// Modem reset에 의해 Join을 다시할 경우 ADR 및 DR 재설정.
		if (!modemCtx.config.setDr) {
			sendAtCommand(MODEM_ATCMD_TIMEOUT_MS, MODEM_ATCMD_RETRY_CNT, &AtCmdSetDr,
				      "0");
			return FALSE;
		}

		if (!modemCtx.config.setAdr) {
			sendAtCommand(MODEM_ATCMD_TIMEOUT_MS, MODEM_ATCMD_RETRY_CNT, &AtCmdSetAdr,
				      "1");
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL provision()
{
	/*
        리셋 후 JOIN : 최대 1시간동안 "JOINED" 패턴을 대기.
        이후 주기 동작 : 바로 다음 단계 진행
         - AT+PS 는 최초 JOIN만 확인하며 두번째 JOIN부터는 완료 여부 확인 불가.
         - 위 이유로 펌웨어 내 FLAG로 판단한다.
    */
	modemCtx.errCode = MODEM_ERROR_JOIN_FAIL;
	if (modemCtx.status.provision) {
		return TRUE;
	}

	if (RTC_isValidDate(&modemCtx.joinTime) == FALSE) {
		RTC_read(&modemCtx.joinTime);
	}

	Date_t now;
	RTC_read(&now);
	long joinWorkingTime = RTC_calcSecDiff(&modemCtx.joinTime, &now);
	printf_ts("MODEM : Join time = %ld/%d sec\n", joinWorkingTime, MODEM_JOIN_TIMEOUT_SEC);

	if (joinWorkingTime > MODEM_JOIN_TIMEOUT_SEC) { // 1 hour delay
		printf_ts("MODEM : Join timeout\n");
		MODEM_stop(FAIL);
	} else {
		// Join 진행 중.
		OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_PROCESS,
				     (uint32)MODEM_DATA_TIMEOUT_MS); // 10sec
	}
	return FALSE;
}

static BOOL dataTransfer()
{
	modemCtx.errCode = MODEM_ERROR_DATA_FAIL;
	if (modemCtx.status.busy) {
		if (modemCtx.status.timeout) {
			modemCtx.dataTimeoutCnt++;
			modemCtx.status.timeout = 0;
			printf_ts("MODEM : data transfer timeout(%d / %d)\n",
				  modemCtx.dataTimeoutCnt, MODEM_TX_RETRY_COUNT);
			if (AppProcess == APP_IMMEDIATE_REPORT ||
			    AppProcess == APP_INITIAL_REPORT) {
				// 초기설치시(APP_INITIAL_REPORT)나 점검시(APP_IMMEDIATE_REPORT)에만 상황 변화를 LCD에 표시
				LCD_redraw();
				MISC_delayMs(100);
				LCD_accessModem();
			}
			OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT,
					     (uint32)MODEM_DATA_TIMEOUT_MS);
		}

		if (modemCtx.dataTimeoutCnt >= MODEM_TX_RETRY_COUNT) {
			printf_ts("MODEM : Data transfer timeout\n");
			MODEM_stop(FAIL);
		}

		return FALSE;
	}

	if (!modemCtx.status.acked) {
		int len = 0;
		char payload[LEN_MAX_LORA_DATA];
		if (AppProcess == APP_IMMEDIATE_REPORT || AppProcess == APP_INITIAL_REPORT) {
			len = MODEM_sendJoin(payload);
		} else {
			len = MODEM_sendDataReport(payload);
		}

		int bufSize = (len * 2) + 1;
		char *buf = (char *)malloc(bufSize); // '1' is end line
		memset(buf, 0, bufSize);

		for (int i = 0; i < len; i++) {
			snprintf(buf, bufSize, "%s%02X", buf, payload[i]);
		}

		modemCtx.dataTimeoutCnt = 0;
		modemCtx.status.timeout = 0;
		sendAtCommand(MODEM_DATA_TIMEOUT_MS, MODEM_DATA_RETRY_CNT, &AtCmdSendData, buf);
		free(buf);

		return FALSE;
	}

	METER_clearStoredData(); // 데이터 전송 완료 후 저장된 검침데이터 삭제.
	return TRUE;
}

static BOOL getSigQuality()
{
	modemCtx.errCode = MODEM_ERROR_NO_RESP;
	if (modemCtx.status.busy) {
		return FALSE;
	}

	if (!modemCtx.status.getSig) {
		sendAtCommand(MODEM_ATCMD_TIMEOUT_MS, MODEM_ATCMD_RETRY_CNT, &AtCmdGetSig, NULL);
		return FALSE;
	}

	char rssiSNR[0x10] = "";
	memset(rssiSNR, ' ', 0x10);
	sprintf(rssiSNR, "-%ddB %d", modem.sigQuality.rssi, modem.sigQuality.snr);
	LCD_displayString(rssiSNR);
	MISC_delayMs(1000);

	return TRUE;
}

static BOOL getTime()
{
	modemCtx.errCode = MODEM_ERROR_TIME_SYNC_FAIL;
	if (modemCtx.status.busy) {
		if (modemCtx.status.timeout) {
			modemCtx.dataTimeoutCnt++;
			modemCtx.status.timeout = 0;
			printf_ts("MODEM : time sync timeout(%d / %d)\n", modemCtx.dataTimeoutCnt,
				  MODEM_TX_RETRY_COUNT);
			if (AppProcess == APP_IMMEDIATE_REPORT ||
			    AppProcess == APP_INITIAL_REPORT) {
				// 초기설치시(APP_INITIAL_REPORT)나 점검시(APP_IMMEDIATE_REPORT)에만 상황 변화를 LCD에 표시
				LCD_redraw();
				MISC_delayMs(100);
				LCD_accessModem();
			}
			OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT,
					     (uint32)MODEM_DATA_TIMEOUT_MS);
		}

		if (modemCtx.dataTimeoutCnt >= MODEM_TX_RETRY_COUNT) {
			printf_ts("MODEM : Time sync timeout\n");
			MODEM_stop(FAIL);
		}

		return FALSE;
	}

	if (!modemCtx.status.timeSync) {
		modemCtx.dataTimeoutCnt = 0;
		modemCtx.status.timeout = 0;
		sendAtCommand(MODEM_DATA_TIMEOUT_MS, MODEM_DATA_RETRY_CNT, &AtCmdGetTime, NULL);
		return FALSE;
	}

	return TRUE;
}

static BOOL getNwkSession()
{
	modemCtx.errCode = MODEM_ERROR_NO_RESP;
	if (modemCtx.status.busy) {
		return FALSE;
	}

	if (!modemCtx.config.getDevEui) {
		sendAtCommand(MODEM_ATCMD_TIMEOUT_MS, MODEM_ATCMD_RETRY_CNT, &AtCmdGetDevEui, NULL);
		return FALSE;
	}

	if (!modemCtx.config.getAppEui) {
		sendAtCommand(MODEM_ATCMD_TIMEOUT_MS, MODEM_ATCMD_RETRY_CNT, &AtCmdGetAppEui, NULL);
		return FALSE;
	}

	if (!modemCtx.config.getAppKey) {
		sendAtCommand(MODEM_ATCMD_TIMEOUT_MS, MODEM_ATCMD_RETRY_CNT, &AtCmdGetAppKey, NULL);
		return FALSE;
	}

	BOOL isValidAppEui = FALSE;
	for (int idx = 0; idx < sizeof(modem.appEui); idx++) {
		if (modem.appEui[idx] != 0) {
			// App EUI array 값이 단 하나라도 '0'이 아닌 경우 유효값으로 판단.
			isValidAppEui = TRUE;
		}
	}

	BOOL isValidAppKey = FALSE;
	for (int idx = 0; idx < sizeof(modem.appKey); idx++) {
		if (modem.appKey[idx] != 0) {
			// App Key array 값이 단 하나라도 '0'이 아닌 경우 유효값으로 판단.
			isValidAppKey = TRUE;
		}
	}

	if (isValidAppKey && isValidAppEui) {
		return TRUE;
	} else {
		// Invalid network session에 의한 실패이므로 Join 실패로 Error Code 설정.
		modemCtx.errCode = MODEM_ERROR_JOIN_FAIL;
		printf_ts("MODEM : Network session data is invalid.\n");
		MODEM_stop(FAIL);
		return FALSE;
	}
}

static BOOL setNwkSession()
{
	modemCtx.errCode = MODEM_ERROR_NO_RESP;
	if (modemCtx.status.busy) {
		return FALSE;
	}

	if (!modemCtx.config.setAppEui) {
		char appEui[17]; // App EUI string len = 16, End line = 1
		memset(appEui, 0, sizeof(appEui));
		for (int idx = 0; idx < 8; idx++) {
			snprintf(appEui, sizeof(appEui), "%s%02X", appEui, modem.userAppEui[idx]);
		}
		sendAtCommand(MODEM_ATCMD_TIMEOUT_MS, MODEM_ATCMD_RETRY_CNT, &AtCmdSetAppEui,
			      appEui);
		return FALSE;
	}

	if (!modemCtx.config.setAppKey) {
		char appKey[33]; // App EUI string len = 32, End line = 1
		memset(appKey, 0, sizeof(appKey));
		for (int idx = 0; idx < 16; idx++) {
			snprintf(appKey, sizeof(appKey), "%s%02X", appKey, modem.userAppKey[idx]);
		}
		sendAtCommand(MODEM_ATCMD_TIMEOUT_MS, MODEM_ATCMD_RETRY_CNT, &AtCmdSetAppKey,
			      appKey);
		return FALSE;
	}

	if (!modemCtx.config.resetNwk) {
		if (modem.lastUserJoin == LORA_SEUDO_JOIN) {
			sendAtCommand(MODEM_ATCMD_TIMEOUT_MS, MODEM_ATCMD_RETRY_CNT, &AtCmdResetNwk,
				      "0");
		} else {
			sendAtCommand(MODEM_ATCMD_TIMEOUT_MS, MODEM_ATCMD_RETRY_CNT, &AtCmdResetNwk,
				      "1");
		}
		return FALSE;
	}

	if (!modemCtx.config.saveCfg) {
		sendAtCommand(MODEM_ATCMD_TIMEOUT_MS, MODEM_ATCMD_RETRY_CNT, &AtCmdSaveCfg, NULL);
		return FALSE;
	}

	return TRUE;
}

static ModemStep_t processDataTransfer()
{
	ModemStep_t nextStep = modemCtx.step;
	switch (modemCtx.step) {
	case MODEM_STEP_IDLE:
		if (idle()) {
			modemCtx.status.alive = 0;
			nextStep = MODEM_STEP_CHECK_ALIVE;
		}
		break;

	case MODEM_STEP_CHECK_ALIVE:
		if (checkAlive()) {
			modemCtx.config.getAppKey = 0;
			modemCtx.config.getAppEui = 0;
			modemCtx.config.getDevEui = 0;
			nextStep = MODEM_STEP_GET_NWK_SESSION;
		}
		break;

	case MODEM_STEP_GET_NWK_SESSION:
		if (getNwkSession()) {
			// Join의 경우 모뎀에서 자동으로 실행되므로 사전에 완료될 수 있다.
			// 때문에 Join flag를 초기화 하지 않는다.
			nextStep = MODEM_STEP_WAIT_PROVISION;
		}
		break;

	case MODEM_STEP_WAIT_PROVISION:
		if (provision()) {
			modemCtx.status.acked = 0;
			nextStep = MODEM_STEP_WAIT_ACK_DATA;
		}
		break;

	case MODEM_STEP_WAIT_ACK_DATA:
		if (dataTransfer()) {
			modemCtx.status.getSig = 0;
			nextStep = MODEM_STEP_GET_SIG;
		}
		break;

	case MODEM_STEP_GET_SIG:
		if (getSigQuality()) {
			modemCtx.status.timeSync = (RTC_needTimeSync() == TRUE) ? 0 : 1;
			nextStep = MODEM_STEP_GET_TIME;
		}
		break;

	case MODEM_STEP_GET_TIME:
		if (getTime()) {
			MODEM_stop(SUCCESS);
			nextStep = MODEM_STEP_IDLE;
		}
		break;

	default: // Unknown state 는 unknown error 로 처리.
		modemCtx.errCode = MODEM_ERROR_UNKNOWN;
		printf_ts("MODEM : Current step is invalid\n");
		MODEM_stop(FAIL);
		break;
	}

	return nextStep;
}

static ModemStep_t processSetNetwork()
{
	ModemStep_t nextStep = modemCtx.step;
	switch (modemCtx.step) {
	case MODEM_STEP_IDLE:
		if (idle()) {
			modemCtx.status.alive = 0;
			nextStep = MODEM_STEP_CHECK_ALIVE;
		}
		break;

	case MODEM_STEP_CHECK_ALIVE:
		if (checkAlive()) {
			modemCtx.config.setAppKey = 0;
			modemCtx.config.setAppEui = 0;
			modemCtx.config.resetNwk = 0;
			modemCtx.config.saveCfg = 0;
			nextStep = MODEM_STEP_SET_NWK_SESSION;
		}
		break;

	case MODEM_STEP_SET_NWK_SESSION:
		if (setNwkSession()) {
			modemCtx.config.getAppKey = 0;
			modemCtx.config.getAppEui = 0;
			modemCtx.config.getDevEui = 0;
			nextStep = MODEM_STEP_GET_NWK_SESSION;
		}
		break;

	case MODEM_STEP_GET_NWK_SESSION:
		if (getNwkSession()) {
			MODEM_stop(SUCCESS);
			MODEM_turnOff(); // Network session 변경으로 모뎀 재시작으로 위해 VCC off.
			nextStep = MODEM_STEP_IDLE;
		}
		break;

	default: // Unknown state 는 unknown error 로 처리.
		modemCtx.errCode = MODEM_ERROR_UNKNOWN;
		printf_ts("MODEM : Current step is invalid\n");
		MODEM_stop(FAIL);
		break;
	}

	return nextStep;
}

BOOL MODEM_process(int appProcess)
{
	if (modemCtx.status.stop) {
		return FALSE;
	}

	ModemStep_t nextStep;
	if (appProcess == APP_SET_CONFIGURATION) {
		nextStep = processSetNetwork();
	} else {
		nextStep = processDataTransfer();
	}

	if (modemCtx.status.stop) {
		// STEP 동작 중 모뎀동작을 종료할 경우 기존 이벤트를 해지.
		// POWER OFF 이벤트는 stop과 함께 등록되므로 등록 생략.
		OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT);
		OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_PROCESS);
	} else {
		if (modemCtx.step != nextStep) {
			modemCtx.step = nextStep;
			OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT);
			OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
		}
	}

	return TRUE;
}

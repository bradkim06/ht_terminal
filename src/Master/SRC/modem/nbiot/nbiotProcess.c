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
#include "uuid.h"
#include "message.h"
#include "nbiotModem.h"

// AT command default timeout/retry
#define AT_CMD_DEFAULT_TIMEOUT 5000
#define AT_CMD_DEFAULT_RETRY 4

// Attach, Detach, No ack UDP timeout/retry
#define AT_CMD_ATTACH_TIMEOUT 10000
#define AT_CMD_ATTACH_RETRY 11

#define AT_CMD_COMM_TIMEOUT 5000
#define AT_CMD_COMM_DETACH_RETRY 0

// Modem Reset timeout/retry
#define AT_CMD_RESET_TIMEOUT 15000
#define AT_CMD_RESET_RETRY 0

// UDP data transfer timeout/retry
#if defined(MODEM_FUN_EN_PSM)
#define AT_CMD_UDP_TIMEOUT 30000
#define AT_CMD_UDP_RETRY 1
#else
#define AT_CMD_UDP_TIMEOUT 30000
#define AT_CMD_UDP_RETRY 1
#endif
// LWM2M data transfer timeout/retry
#define AT_CMD_LWM2M_TIMEOUT 60000
#define AT_CMD_LWM2M_RETRY 0

// MODEM remote open timeout/retry
#define MODEM_CHECK_ALIVE_TIMEOUT 10000
#define MODEM_CHECK_ALIVE_RETRY 30

#define MODEM_ENTER_PSM_TIMEOUT 30000
#define MODEM_FW_DOWN_REQ_TIMEOUT 40000
#define MODEM_FW_DOWNLOAD_TIMEOUT 300000
#define MODEM_FW_UPGRADE_TIMEOUT 150000
#define MODEM_FOTA_COMPLETE_WAIT 60000

#define MODEM_EVENT_INTERVAL 1000
#define MODEM_BOOTUP_INTERVAL 7000

#define END_STEP_FLOW_INDEX 0xFF
#define ENTER_PSM_FLOW_INDEX 0xFE
#define WAIT_PSM_FLOW_INDEX 0xFD

extern Config_t conf;
extern Modem_t modem;

ModemComm_t modemComm;
ModemContext_t modemCtx;
char dlIp[SERVER_IP_STR_LEN];

uchar raw_data[LEN_MAX_NBIOT_DATA];
char ul_data[320];

typedef struct {
	uint8 idle;
	uint8 bip;
	uint8 init;
	uint8 attach;
	uint8 updateQa;
	uint8 transfer;
	uint8 certify;
	uint8 fota;
	uint8 detach;
	uint8 retry;
} StepFlowIndex_t;
static StepFlowIndex_t StepFlowIndex;

// NB-IoT Modem AT command list
static AtCmd_t AtCmdSwReset = { AT_CMD_IDX_SW_RESET, "AT+NRB" };
static AtCmd_t AtCmdDetachNw = { AT_CMD_IDX_DETACH_NW, "AT+CGATT" };
static AtCmd_t AtCmdGetIMEI = { AT_CMD_IDX_GET_IMEI, "AT+CGSN" };
static AtCmd_t AtCmdGetIMSI = { AT_CMD_IDX_GET_IMSI, "AT+CIMI" };
static AtCmd_t AtCmdGetICCID = { AT_CMD_IDX_GET_ICCID, "AT+NCCID" };
static AtCmd_t AtCmdGetRadioQuality = { AT_CMD_IDX_GET_RADIO_QUALITY, "AT+NUESTATS" };
static AtCmd_t AtCmdGetNwAlarm = { AT_CMD_IDX_GET_NW_ALARM, "AT+CEREG?" };
static AtCmd_t AtCmdGetTime = { AT_CMD_IDX_GET_TIME, "AT+CCLK?" };
static AtCmd_t AtCmdSetRfCtrl = { AT_CMD_IDX_SET_RF_CTRL, "AT+CFUN" };
static AtCmd_t AtCmdSetPSM = { AT_CMD_IDX_SET_PSM, "AT+CPSMS" };
static AtCmd_t AtCmdSetBAND = { AT_CMD_IDX_SET_BAND, "AT+NBAND" };
static AtCmd_t AtCmdSetReselect = { AT_CMD_IDX_SET_RESELECT, "AT+NCONFIG" };
static AtCmd_t AtCmdSetNwAlarm = { AT_CMD_IDX_SET_NW_ALARM, "AT+CEREG" };
static AtCmd_t AtCmdSocketCreate = { AT_CMD_IDX_SOCKET_CREATE, "AT+NSOCR" };
static AtCmd_t AtCmdSocketSendUL = { AT_CMD_IDX_SOCKET_SEND_UL, "AT+NSOST" };
static AtCmd_t AtCmdSocketClose = { AT_CMD_IDX_SOCKET_CLOSE, "AT+NSOCL" };
static AtCmd_t AtCmdSetReportPSM = { AT_CMD_IDX_SET_REPORT_PSM, "AT+NPSMR" };
#if defined(NBIOT_BASE_TYPE)
static AtCmd_t AtCmdSocketRecvDL = { AT_CMD_IDX_SOCKET_RECV_DL, "AT+NSORF" };
#endif

static AtCmd_t AtCmdGetFwRev = { AT_CMD_IDX_GET_FW_REV, "AT+QGMR" };
static AtCmd_t AtCmdSetLWM2M = { AT_CMD_IDX_SET_LWM2M, "AT+QBOOTSTRAPHOLDOFF" };
static AtCmd_t AtCmdSetLWServer = { AT_CMD_IDX_SET_LWM2M_SERVER, "AT+QLWSERVERIP" };
static AtCmd_t AtCmdSetEPN = { AT_CMD_IDX_SET_EPN, "AT+QLWEPNS" };
static AtCmd_t AtCmdSetBSPS = { AT_CMD_IDX_SET_BSPS, "AT+QLWMBSPS" };
static AtCmd_t AtCmdGetLWServer = { AT_CMD_IDX_GET_LWM2M_SERVER, "AT+QLWSERVERIP?" };
static AtCmd_t AtCmdGetBSPS = { AT_CMD_IDX_GET_BSPS, "AT+QLWMBSPS?" };
static AtCmd_t AtCmdGetEPN = { AT_CMD_IDX_GET_EPN, "AT+QLWEPNS?" };
static AtCmd_t AtCmdSwtLWM2M = { AT_CMD_IDX_SWITCH_LWM2M, "AT+QREGSWT" };
static AtCmd_t AtCmdGetSwtLWM2M = { AT_CMD_IDX_GET_SWITCH_LWM2M, "AT+QREGSWT?" };
static AtCmd_t AtCmdSetFOTA = { AT_CMD_IDX_SET_FOTA, "AT+QLWFOTAIND" };
static AtCmd_t AtCmdRunRegister = { AT_CMD_IDX_RUN_REGISTER, "AT+QLWSREGIND" };
#if defined(NBIOT_LG_TYPE)
static AtCmd_t AtCmdRunDataNoti = { AT_CMD_IDX_RUN_DATA_NOTI, "AT+QLWULDATA" };
#endif

/**
 * @brief Get the End Point Name(= CSE ID) object
 *
 * @param service_code  LG U+ Platform service code
 * @param ctnStr        USIM CTN (format is string)
 * @param iccidStr      USIM ICCID (format is string)
 * @return char*        End point string (It is allocated on the heap. so, have to free)
 */
static char *getEndPointName(const char *serviceCode, const char *ctnStr, const char *iccidStr)
{
#define LEN_SHORT_UUID 10
#define LEN_UUID_PARAM (LEN_MODEM_CTN + LEN_MODEM_ICCID)

	char uuidParam[LEN_UUID_PARAM + 1];
	char uuid[LEN_MODEM_UUID + 1];
	char shortUuid[LEN_SHORT_UUID + 1];

	// Make parameter for making Endpoint Name
	snprintf(uuidParam, LEN_UUID_PARAM + 1, "%s%s", ctnStr, iccidStr);

	// Make UUID
	uuid_t u;
	uuid_create_md5_from_name(&u, uuidParam, LEN_UUID_PARAM);
	snprintf(uuid, LEN_MODEM_UUID + 1, "%8.8lx-%4.4x-%4.4x-%2.2x%2.2x-", u.time_low, u.time_mid,
		 u.time_hi_and_version, u.clock_seq_hi_and_reserved, u.clock_seq_low);
	for (int i = 0; i < 6; i++) {
		snprintf(uuid, LEN_MODEM_UUID + 1, "%s%2.2x", uuid, u.node[i]);
	}

	// Make Short UUID from UUID
	snprintf(shortUuid, 6, "%s", &uuid[0]); // UUDI의 앞 5자리 Copy
	snprintf(&shortUuid[5], 6, "%s",
		 &uuid[LEN_MODEM_UUID - 5]); // UUDI의 뒤 5자리 Copy

	// Make Endpoint Name
	memset(ul_data, 0, sizeof(ul_data));
	snprintf(ul_data, LEN_MODEM_EP_NAME + 1, "ASN_CSE-D-%s-%s", shortUuid, serviceCode);

	return ul_data;
}

/**
 * @brief Set the Modem data object for operating
 */
static void setModemData()
{
	// Check and Copy IMEI
	printf_ts("IMEI    : %s", modem.imeiStr);

	uchar imei[8];
	MODEM_copyImei(imei);
	if (memcmp(conf.imei, imei, 8) != 0) {
		memcpy(conf.imei, imei, 8);
		FLASH_saveConfigInfo(&conf);
		printf(" - changed IMEI\n");
	} else {
		printf("\n");
	}

	// Check IMSI
	printf_ts("IMSI    : %s", modem.imsiStr);
	if (modem.modemType == MODEM_TYPE_KT) {
		printf("(KT)\n");
	} else if (modem.modemType == MODEM_TYPE_UPLUS) {
		printf("(UPLUS)\n");
	} else {
		printf("(UNKNOWN)\n");
	}

	// Make CTN
	snprintf(modem.ctnStr, LEN_MODEM_CTN + 1, "0%s", &modem.imsiStr[5]);
	printf_ts("CTN     : %s\n", modem.ctnStr);
	printf_ts("ICCID   : %s\n", modem.iccidStr);

	// Make End point name (= CSE ID)
	char *epName = getEndPointName(conf.serviceCode, modem.ctnStr, modem.iccidStr);
	memcpy(modem.epName, epName, sizeof(modem.epName));

	printf_ts("Endpoint : %s\n", modem.epName);

	// Make Bootstrap paramter
	char model[TERM_MODEL_STRING_LEN + 1];
	if (SMART_WATER_METER(conf.termModel)) {
		memcpy(model, "HTM-115Q", TERM_MODEL_STRING_LEN + 1);
	} else {
		memcpy(model, "HAT-124Q", TERM_MODEL_STRING_LEN + 1);
	}

	char devSn[11];
	// Bootstrap 상의 serial number 는 ICCID의 10자리 사용.
	// USIM Model num 4 + Usim S/N num 6
	// EX : ICCID = 8982068086002814399 >> devSn = 8600281439
	memcpy(devSn, &modem.iccidStr[LEN_MODEM_ICCID - 11], 10);
	devSn[10] = 0; // Last char is set string end

	snprintf(modem.bsParam, LEN_MODEM_BS_PARAM + 1, "%s,%s,%s,%s,%s", conf.serviceCode, devSn,
		 modem.ctnStr, &modem.iccidStr[LEN_MODEM_ICCID - 6], model);

	printf_ts("BS param : %s\n", modem.bsParam);
}

/**
 * @brief AT command send wrapper function
 *
 * @param timeout timeout(ms) of each AT command
 * @param retry   AT command retry count
 * @param cmd     AT command (string)
 * @param param   Parameter (string, support format)
 * @param ...     Parameter format argument
 */
static void sendAtCommand(uint32 timeout, int retry, AtCmd_t *atCmd, char *param, ...)
{
	ASSERT_PRINT(!(modemCtx.status.busy), "BUSY flag is not released \n");
	ASSERT_PRINT((atCmd->cmd != NULL && strstr(atCmd->cmd, "AT") != NULL),
		     "AT Command string is NULL\n");

	if (retry >= 0) {
		snprintf(modemComm.atData, LEN_MAX_AT_DATA, "%s", atCmd->cmd);

		if (param != NULL) {
			va_list arg;
			va_start(arg, param);
			int paramLen =
				vsnprintf((modemComm.atData + strlen(atCmd->cmd)),
					  (LEN_MAX_AT_DATA - strlen(atCmd->cmd)), param, arg);
			ASSERT_PRINT((paramLen > 0), "Formatting parameter converting fail\n");
			va_end(arg);
		}

		modemComm.atCmd = atCmd;
		modemComm.retry = retry;
		modemComm.timeout = timeout;

		modemCtx.status.busy = 1;
		modemCtx.status.error = 0;
		MODEM_write(modemComm.atData);

		OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT, modemComm.timeout);
	}
}

/**
 * @brief AT command send wrapper function (no wait response)
 *
 * @param delay   delay(ms) after sending each AT command
 * @param cmd     AT command (string)
 * @param param   Parameter (string, support format)
 * @param ...     Parameter format argument
 */
static void sendNoRespAtCommand(uint32 delay, AtCmd_t *atCmd, char *param, ...)
{
	ASSERT_PRINT((atCmd->cmd != NULL && strstr(atCmd->cmd, "AT") != NULL),
		     "AT Command string is NULL\n");

	snprintf(modemComm.atData, LEN_MAX_AT_DATA, "%s", atCmd->cmd);

	if (param != NULL) {
		va_list arg;
		va_start(arg, param);
		int paramLen = vsnprintf((modemComm.atData + strlen(atCmd->cmd)),
					 (LEN_MAX_AT_DATA - strlen(atCmd->cmd)), param, arg);
		ASSERT_PRINT((paramLen > 0), "Formatting parameter converting fail\n");
		va_end(arg);
	}

	modemComm.atCmd = atCmd;
	modemComm.retry = 0;
	modemComm.timeout = 0;

	modemCtx.status.busy = 0;
	MODEM_write(modemComm.atData);

	MISC_delayMs(delay);
}

/**
 * @brief Get the Qa Report Message object
 *        Return message is allocated on the heap. so, have to free
 *
 * @param messageLen QA report message length
 * @return char*     QA report message (converted to string)
 */
static char *getQaReportMessage(int *messageLen)
{
	memset(raw_data, 0, sizeof(raw_data));
	int len = MODEM_qaData(raw_data);

	// Hex data를 ASCII로 변환하여 저장할 버퍼의 길이이므로
	// 본래 데이터 길이의 x2에 '\0'를 위한 +1로 정의.
	int dataLen = (len * 2) + 1;
	memset(ul_data, 0, sizeof(ul_data));

	for (int i = 0; i < len; i++) {
		snprintf(ul_data, dataLen, "%s%02X", ul_data, *(raw_data + i));
	}

	*messageLen = len;
	return ul_data;
}

/**
 * @brief Get the Error Report Message object
 *        Return message is allocated on the heap. so, have to free
 *
 * @param messageLen Error report message length
 * @return char*     Error report message (converted to stringe)
 */
static char *getErrorReportMessage(int *messageLen)
{
	memset(raw_data, 0, sizeof(raw_data));
	int len = MODEM_errData(raw_data, modemCtx.pfUlCnt, modemCtx.pfDlCnt, modemCtx.ulCnt,
				modemCtx.dlCnt);

	// Hex data를 ASCII로 변환하여 저장할 버퍼의 길이이므로
	// 본래 데이터 길이의 x2에 '\0'를 위한 +1로 정의.
	int dataLen = (len * 2) + 1;
	memset(ul_data, 0, sizeof(ul_data));

	for (int i = 0; i < len; i++) {
		snprintf(ul_data, dataLen, "%s%02X", ul_data, *(raw_data + i));
	}

	*messageLen = len;
	return ul_data;
}

/**
 * @brief Get the Meter Data Message object
 *        Return message is allocated on the heap. so, have to free
 *
 * @param isJoin     Flag specifying whether the message is a JOIN message or not.
 * @param messageLen Meter data report message length
 * @return char*     Meter data report message (converted to string)
 */
static char *getMeterDataMessage(BOOL isJoin, int *messageLen)
{
	memset(raw_data, 0, LEN_MAX_NBIOT_DATA);
	int len;
	if (isJoin) {
		len = MODEM_sendJoin(raw_data);
	} else {
		len = MODEM_sendPeriodicData(raw_data);
	}

	// Hex data를 ASCII로 변환하여 저장할 버퍼의 길이이므로
	// 본래 데이터 길이의 x2에 '\0'를 위한 +1로 정의.
	memset(ul_data, 0, sizeof(ul_data));

	for (int i = 0; i < len; i++) {
		snprintf(ul_data, LEN_MAX_AT_DATA, "%s%02X", ul_data, *(raw_data + i));
	}

	*messageLen = len;
	return ul_data;
}

/**
 * @brief IDLE step operator
 *
 * @return ModemStep_t    return next step
 */
static ModemStep_t idle()
{
	ASSERT_PRINT(modemCtx.step == MODEM_STEP_IDLE, "step is invalid \n");

	ModemStep_t nextStep =
		modemCtx.step; // step 종료 전 까지 현재 step을 next step 으로 return.
	modemCtx.errCode = MODEM_ERROR_NONE;
	modemCtx.retryCount = 0;
	modemCtx.retryStep = MODEM_STEP_UNKNOWN;

	modemCtx.proc.value = 0;

	if (RTC_isTimeSync()) {
		Date_t now;
		RTC_read(&now);
		if (RTC_calcHourDiff(&modem.lastUpdateTime, &now) >= LG_QA_HOUR_INTERVAL) {
			modemCtx.proc.updateQa = 1;
		}
		int serialBase = ascii2Hex(conf.serialNum[8]) * 1000 +
				 ascii2Hex(conf.serialNum[9]) * 100 +
				 ascii2Hex(conf.serialNum[10]) * 10 + ascii2Hex(conf.serialNum[11]);

		if (conf.fotaInterval > 0) {
			if ((now.day % conf.fotaInterval) == (serialBase % conf.fotaInterval)) {
				if (now.hour == conf.intervalBaseTime) {
					modemCtx.proc.runFota = 1;
				}
			}
		} else {
			modemCtx.proc.runFota = 1;
		}
	} else {
		// 현재 단말기 동작이 최초 부팅 동작일 경우 FOTA 진행.
		if (!conf.isModemInit) {
			modemCtx.proc.runFota = 1;
		}
		modemCtx.proc.updateQa = 1;
	}

	if (modemCtx.proc.updateQa) {
		printf_ts("PROC : Enable Update QA.\n");
	}
	if (modemCtx.proc.updateReg) {
		printf_ts("PROC : Enable Update Register.\n");
	}
	if (modemCtx.proc.runFota) {
		printf_ts("PROC : Enable FOTA.\n");
		// FOTA 수행을 위한 Flag 초기화
		modemCtx.lwm2m.fotaDownReq = 0;
		modemCtx.lwm2m.fotaUpgradeReq = 0;
		modemCtx.lwm2m.fotaFinish = 0;
		modemCtx.proc.runInit = 1;
		nextStep = MODEM_STEP_BIP;
	} else {
		// Check previous Modem state with enable sleep flag.
		modemCtx.proc.runInit = (conf.isModemInit) ? 0 : 1;
		nextStep = MODEM_STEP_BIP;
	}
	if (modemCtx.proc.runInit) {
		printf_ts("PROC : Enable Modem Init.\n");
	}

	return nextStep;
}

/**
 * @brief BIP step operator (V120의 경우 BIP를 지원하지 않으므로 모뎀동작 여부만 확인.)
 *
 * @return ModemStep_t    return next step
 */
static ModemStep_t bip()
{
	ASSERT_PRINT(modemCtx.step == MODEM_STEP_BIP, "step is invalid \n");

	ModemStep_t nextStep =
		modemCtx.step; // step 종료 전 까지 현재 step을 next step 으로 return.
	modemCtx.errCode = MODEM_ERROR_USIM_INVALID;

	if (modemCtx.stepReset) {
		StepFlowIndex.bip = 0;
	}

	static uint32 startEventTimeMsec = 0;
	switch (StepFlowIndex.bip) {
	case 0: {
		startEventTimeMsec = TIMER_getMsec();
		OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
		StepFlowIndex.bip++;
	} break;

	case 1: {
		if (TIMER_getMsecDiff(startEventTimeMsec) >= MODEM_BOOTUP_INTERVAL) {
			OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
			StepFlowIndex.bip++;
		} else {
			OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_PROCESS,
					     MODEM_EVENT_INTERVAL);
		}
	} break;

	case 2: {
		sendAtCommand(MODEM_CHECK_ALIVE_TIMEOUT, MODEM_CHECK_ALIVE_RETRY, &AtCmdGetIMSI,
			      NULL);
		StepFlowIndex.bip++;
	} break;

	case END_STEP_FLOW_INDEX:
	default: {
		uchar imsi[8];
		MODEM_copyImsi(imsi);
		if (memcmp(conf.imsi, imsi, 8) != 0) {
			memcpy(conf.imsi, imsi, 8);
			printf_ts("MODEM : changed USIM\n");
			modem.lastAttachStatus = MODEM_NW_STATUS_NORMAL;
		}

		if (modem.lastAttachStatus == MODEM_NW_STATUS_PAUSED) {
			printf_ts("MODEM : cellreg paused - try attach\n");
		} else if (modem.lastAttachStatus == MODEM_NW_STATUS_REJECTED) {
			printf_ts("MODEM : cellreg rejected - wait change USIM\n");
		}
		nextStep = MODEM_STEP_INIT;
	} break;
	}

	return nextStep;
}

/**
 * @brief BC95G modem INIT step operator
 *
 * @return ModemStep_t    return next step
 */
static ModemStep_t init()
{
	ASSERT_PRINT(modemCtx.step == MODEM_STEP_INIT, "step is invalid \n");

	ModemStep_t nextStep =
		modemCtx.step; // step 종료 전 까지 현재 step을 next step 으로 return.
	modemCtx.errCode = MODEM_ERROR_AT_CMD_NO_RESP;

	if (modemCtx.stepReset) {
		StepFlowIndex.init = 0;
	}

	switch (StepFlowIndex.init) {
	case 0: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdGetICCID, NULL);
		StepFlowIndex.init++;
	} break;

	case 1: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdGetIMEI, "=1");
		StepFlowIndex.init++;
	} break;

	case 2: {
		setModemData();
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdGetLWServer,
			      NULL);
		StepFlowIndex.init++;
	} break;

	case 3: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdGetEPN, NULL);
		StepFlowIndex.init++;
	} break;

	case 4: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdGetBSPS, NULL);
		StepFlowIndex.init++;
	} break;

	case 5: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdGetSwtLWM2M,
			      NULL);
		StepFlowIndex.init++;
	} break;

	case 6: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdGetFwRev, NULL);
		StepFlowIndex.init =
			(modemCtx.proc.runInit) ? (StepFlowIndex.init + 1) : END_STEP_FLOW_INDEX;
	} break;

	case 7: {
		sendAtCommand(AT_CMD_RESET_TIMEOUT, AT_CMD_RESET_RETRY, &AtCmdSwReset, NULL);
		StepFlowIndex.init++;
	} break;

	case 8: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSetRfCtrl, "=0");
		StepFlowIndex.init++;
	} break;

	case 9: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSetBAND, "=5");
		StepFlowIndex.init++;
	} break;

	case 10: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSetPSM, "=0");
		StepFlowIndex.init++;
	} break;

	case 11: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSetReselect,
			      "=CELL_RESELECTION,TRUE");
		StepFlowIndex.init++;
	} break;

	case 12: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSetLWServer,
			      "=BS,%s,%d", conf.fotaIp, conf.fotaPort);
		StepFlowIndex.init++;
	} break;

	case 13: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSetLWServer,
			      "=LWM2M,%s,%d", conf.fotaIp, conf.fotaPort);
		StepFlowIndex.init++;
	} break;

	case 14: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSetEPN,
			      "=1,\"%s\"", modem.epName);
		StepFlowIndex.init++;
	} break;

	case 15: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSetBSPS, "=%s",
			      modem.bsParam);
		StepFlowIndex.init++; // BS start after 15sec after attach complete
	} break;

	case 16: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSetLWM2M, "=0");
		StepFlowIndex.init++;
	} break;

	case 17: {
		if (modemCtx.status.lwm2mOn) {
			sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSetFOTA,
				      "=1");
		} else {
			OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
		}
		StepFlowIndex.init++;
	} break;

	case 18: {
		if (modemCtx.proc.runFota) {
			sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSwtLWM2M,
				      "=0");
		} else {
			sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSwtLWM2M,
				      "=2");
		}
		StepFlowIndex.init++;
	} break;

	case 19: {
		sendAtCommand(AT_CMD_RESET_TIMEOUT, AT_CMD_RESET_RETRY, &AtCmdSwReset, NULL);
		StepFlowIndex.init++;
	} break;

	case END_STEP_FLOW_INDEX:
	default: {
		if (!conf.isModemInit) {
			conf.isModemInit = TRUE;
			FLASH_saveConfigInfo(&conf);
		}

		// 모듈에서 자동으로 수행하는 통신(LWM2M 등)을 초기화 하기 위해 Reset.
		modemCtx.proc.runInit = 0;
		nextStep = MODEM_STEP_ATTACH_NW;
	} break;
	}

	return nextStep;
}

/**
 * @brief Attach step operator
 *
 * @return ModemStep_t    return next step
 */
static ModemStep_t attachNw()
{
	ASSERT_PRINT(modemCtx.step == MODEM_STEP_ATTACH_NW, "step is invalid \n");

	ModemStep_t nextStep =
		modemCtx.step; // step 종료 전 까지 현재 step을 next step 으로 return.
	modemCtx.errCode = MODEM_ERROR_AT_CMD_NO_RESP;

	if (modemCtx.stepReset) {
		StepFlowIndex.attach = 0;
	}

	switch (StepFlowIndex.attach) {
	case 0: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSetRfCtrl, "=1");
		StepFlowIndex.attach++;
	} break;

	case 1: {
		sendAtCommand(AT_CMD_COMM_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdDetachNw, "=1");
		StepFlowIndex.attach++;
	} break;

	case 2: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSetNwAlarm, "=5");
		StepFlowIndex.attach++;
	} break;

	case 3: {
		modemCtx.errCode = MODEM_ERROR_ATTACH_FAIL;
		sendAtCommand(AT_CMD_ATTACH_TIMEOUT, AT_CMD_ATTACH_RETRY, &AtCmdGetNwAlarm, NULL);
		StepFlowIndex.attach++;
	} break;

	case 4: {
		sendAtCommand(AT_CMD_ATTACH_TIMEOUT, AT_CMD_ATTACH_RETRY, &AtCmdGetTime, NULL);
		StepFlowIndex.attach++;
	} break;

	case 5: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdGetRadioQuality,
			      "=RADIO");
		StepFlowIndex.attach++;
	} break;

	case 6: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSetReportPSM,
			      "=1");
		StepFlowIndex.attach++;
	} break;

	default: {
		printf_ts(
			"MODEM : CGI(%ld) CID(%d) TX_POWER(%d) RSRP(%d) SNR(%d) RSRQ(%d) RSSI(%d) \n",
			modem.modemQuality.cgi, modem.modemQuality.cid, modem.modemQuality.txPower,
			modem.modemQuality.rsrp, modem.modemQuality.snr, modem.modemQuality.rsrq,
			modem.modemQuality.lastRSSI);

		if (modemCtx.retryStep != MODEM_STEP_UNKNOWN &&
		    modemCtx.retryStep != MODEM_STEP_ATTACH_NW) {
			nextStep = modemCtx.retryStep;
		} else {
			if (modemCtx.proc.updateQa) {
				nextStep = MODEM_STEP_UPDATE_QA;
			} else {
				nextStep = MODEM_STEP_TRANSFER;
			}
		}
	} break;
	}

	// step 종료 전 까지 현재 step을 next step 으로 return.
	return nextStep;
}

/**
 * @brief QA uplink send step operator
 *
 * @return ModemStep_t    return next step
 */
static ModemStep_t updateQa()
{
	ASSERT_PRINT(modemCtx.step == MODEM_STEP_UPDATE_QA, "step is invalid \n");

	ModemStep_t nextStep =
		modemCtx.step; // step 종료 전 까지 현재 step을 next step 으로 return.
	modemCtx.errCode = MODEM_ERROR_AT_CMD_NO_RESP;

	if (modemCtx.stepReset) {
		StepFlowIndex.updateQa = 0;
	}

	static uint32 startEventTimeMsec = 0;
	switch (StepFlowIndex.updateQa) {
	case 0: {
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSocketCreate,
			      "=DGRAM,17,560,0");
		StepFlowIndex.updateQa++;
	} break;

	case 1: {
		int len;
		char *data = getQaReportMessage(&len);

		modemCtx.errCode = MODEM_ERROR_UDP_UL_FAIL;
		sendAtCommand(AT_CMD_COMM_TIMEOUT, 0, &AtCmdSocketSendUL, "=%d,%s,%ld,%d,%s",
			      modemCtx.socket, LG_QA_SERVER_IP, LG_QA_SERVER_PORT, len, data);

		// 데이터 전송 후 다음 전송까지의 interval계산을 위해 현재 msec 저장.
		startEventTimeMsec = TIMER_getMsec();
		StepFlowIndex.updateQa++;
	} break;

	case 2: {
		if (TIMER_getMsecDiff(startEventTimeMsec) >= AT_CMD_COMM_TIMEOUT) {
			OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
			StepFlowIndex.updateQa++;
		} else {
			OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_PROCESS,
					     MODEM_EVENT_INTERVAL);
		}
	} break;

	case 3: {
		int len;
		char *data = getErrorReportMessage(&len);

		modemCtx.errCode = MODEM_ERROR_UDP_UL_FAIL;
		sendAtCommand(AT_CMD_COMM_TIMEOUT, 0, &AtCmdSocketSendUL, "=%d,%s,%ld,%d,%s",
			      modemCtx.socket, LG_ERR_SERVER_IP, LG_ERR_SERVER_PORT, len, data);

		// 데이터 전송 후 다음 전송까지의 interval계산을 위해 현재 msec 저장.
		startEventTimeMsec = TIMER_getMsec();
		StepFlowIndex.updateQa++;
	} break;

	case 4: {
		if (TIMER_getMsecDiff(startEventTimeMsec) >= AT_CMD_COMM_TIMEOUT) {
			OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
			StepFlowIndex.updateQa++;
		} else {
			OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_PROCESS,
					     MODEM_EVENT_INTERVAL);
		}
	} break;

	case 5: {
		modemCtx.ulCnt = modemCtx.dlCnt = 0;
		modemCtx.pfUlCnt = modemCtx.pfDlCnt = 0;
		memset(modemCtx.errLog, 0, sizeof(modemCtx.errLog));
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSocketClose,
			      "=%d", modemCtx.socket);
		StepFlowIndex.updateQa++;
	} break;

	default: {
		nextStep = MODEM_STEP_TRANSFER;
		RTC_read(&modem.lastUpdateTime);

		modemCtx.proc.updateQa = 0;
	} break;
	}

	return nextStep;
}

/**
 * @brief Transfer step operator
 *
 * @return ModemStep_t    return next step
 */
static ModemStep_t transfer()
{
	ASSERT_PRINT(modemCtx.step == MODEM_STEP_TRANSFER, "step is invalid \n");
	static uint16 transfer_count = 0;

	ModemStep_t nextStep =
		modemCtx.step; // step 종료 전 까지 현재 step을 next step 으로 return.
	modemCtx.errCode = MODEM_ERROR_AT_CMD_NO_RESP;

	if (modemCtx.stepReset) {
		StepFlowIndex.transfer = 0;
		strncpy(dlIp, conf.serverIp, SERVER_IP_STR_LEN);
		printf_ts("transfer count : %d\n", ++transfer_count);
	}

#if defined(MODEM_FUN_EN_PSM)
	static uint32 startEventTimeMsec = 0;
#endif
	// These parameters are for changing server address
	static char newIp[SERVER_IP_STR_LEN];
	static uint16 newPort;
	switch (StepFlowIndex.transfer) {
	case 0: {
		// datagram, udp, port no: 560, rx enable
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSocketCreate,
			      "=DGRAM,17,560,1");
		StepFlowIndex.transfer++;
	} break;

	case 1: {
		int len;
		extern uint8 AppProcess;
		BOOL isJoin = (AppProcess == APP_PERIODIC_REPORT) ? FALSE : TRUE;
		char *data = getMeterDataMessage(isJoin, &len);

		// UL 데이터 전송에 한해서 1회 재시도만 수행. (총 2회 데이터 전송.)
		modemCtx.errCode = MODEM_ERROR_UDP_UL_FAIL;
		sendAtCommand(AT_CMD_UDP_TIMEOUT, AT_CMD_UDP_RETRY, &AtCmdSocketSendUL,
			      "=%d,%s,%d,%d,%s", modemCtx.socket, (char *)conf.serverIp,
			      conf.serverPort, len, data);

		StepFlowIndex.transfer++;
	} break;

	case 2: {
		modemCtx.errCode = MODEM_ERROR_UDP_DL_FAIL;
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSocketRecvDL,
			      "=%d,%d", modemCtx.socket, modemComm.dlDataLen);
		StepFlowIndex.transfer++;
	} break;

	case 3: {
		memset(newIp, 0, SERVER_IP_STR_LEN);
		if (MODEM_recvCmdChangeServer(modemComm.dlData, modemComm.dlDataLen, newIp,
					      &newPort)) {
			strncpy(dlIp, newIp, SERVER_IP_STR_LEN);
			// 직전에 전송한 데이터와 동일한 데이터를 재생성하여 전송.
			int len;
			extern uint8 AppProcess;
			BOOL isJoin = (AppProcess == APP_PERIODIC_REPORT) ? FALSE : TRUE;
			char *data = getMeterDataMessage(isJoin, &len);

			// UL 데이터 전송에 한해서 1회 재시도만 수행. (총 2회 데이터 전송.)
			modemCtx.errCode = MODEM_ERROR_UDP_UL_FAIL;
			sendAtCommand(AT_CMD_UDP_TIMEOUT, AT_CMD_UDP_RETRY, &AtCmdSocketSendUL,
				      "=%d,%s,%d,%d,%s", modemCtx.socket, newIp, newPort, len,
				      data);

			StepFlowIndex.transfer++;
		} else {
			MODEM_recvAck(modemComm.dlData, modemComm.dlDataLen);
			sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY,
				      &AtCmdSocketClose, "=%d", modemCtx.socket);
#if defined(MODEM_FUN_EN_PSM)
			if (modemCtx.proc.runFota) {
				StepFlowIndex.transfer = END_STEP_FLOW_INDEX;
			} else {
				modemCtx.status.psmOn = 0;
				StepFlowIndex.transfer = WAIT_PSM_FLOW_INDEX;
				startEventTimeMsec = TIMER_getMsec();
			}
#else
			StepFlowIndex.transfer = END_STEP_FLOW_INDEX;
#endif
		}
	} break;

	case 4: {
		modemCtx.errCode = MODEM_ERROR_UDP_DL_FAIL;
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSocketRecvDL,
			      "=%d,%d", modemCtx.socket, modemComm.dlDataLen);
		StepFlowIndex.transfer++;
	} break;

	case 5: {
		if (MODEM_recvAck(modemComm.dlData, modemComm.dlDataLen)) {
			strncpy(conf.serverIp, newIp, SERVER_IP_STR_LEN);
			conf.serverPort = newPort;
			FLASH_saveConfigInfo(&conf);
			printf_ts("DL : apply request change server(%s:%d)\n", conf.serverIp,
				  conf.serverPort);
		}
		sendAtCommand(AT_CMD_DEFAULT_TIMEOUT, AT_CMD_DEFAULT_RETRY, &AtCmdSocketClose,
			      "=%d", modemCtx.socket);
#if defined(MODEM_FUN_EN_PSM)
		if (modemCtx.proc.runFota) {
			StepFlowIndex.transfer = END_STEP_FLOW_INDEX;
		} else {
			modemCtx.status.psmOn = 0;
			StepFlowIndex.transfer = WAIT_PSM_FLOW_INDEX;
			startEventTimeMsec = TIMER_getMsec();
		}
#else
		StepFlowIndex.transfer = END_STEP_FLOW_INDEX;
#endif
	} break;

#if defined(MODEM_FUN_EN_PSM)
	case WAIT_PSM_FLOW_INDEX: {
		if (modemCtx.status.psmOn) {
			OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
			StepFlowIndex.transfer = ENTER_PSM_FLOW_INDEX;
		} else {
			if (TIMER_getMsecDiff(startEventTimeMsec) >= MODEM_ENTER_PSM_TIMEOUT) {
				modemCtx.errCode = MODEM_ERROR_AT_CMD_NO_RESP;
				MODEM_stop(FAIL);
			} else {
				OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_PROCESS,
						     MODEM_EVENT_INTERVAL);
			}
		}
	} break;

	case ENTER_PSM_FLOW_INDEX: {
		nextStep = MODEM_STEP_IDLE;
	} break;
#endif

	case END_STEP_FLOW_INDEX:
	default: {
		if (modemCtx.proc.runFota) {
			nextStep = MODEM_STEP_CERTIFY;
		} else {
			nextStep = MODEM_STEP_DETACH_NW;
		}
	} break;
	}

	return nextStep;
}

/**
 * @brief BC95G modem certify step operator (통신사 플랫폼 인증 과정)
 *
 * @return ModemStep_t    return next step
 */
static ModemStep_t certify()
{
	ASSERT_PRINT(modemCtx.step == MODEM_STEP_CERTIFY, "step is invalid \n");

	ModemStep_t nextStep = modemCtx.step;
	modemCtx.errCode = MODEM_ERROR_PF_CERITY_FAIL;

	if (modemCtx.stepReset) {
		StepFlowIndex.certify = 0;
	}

	static uint32 startEventTimeMsec = 0;
	switch (StepFlowIndex.certify) {
	case 0: {
		startEventTimeMsec = TIMER_getMsec();
		OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
		StepFlowIndex.certify++;
	} break;

	case 1: {
		if (modemCtx.lwm2m.bsFinish) {
			sendAtCommand(AT_CMD_LWM2M_TIMEOUT, AT_CMD_LWM2M_RETRY, &AtCmdRunRegister,
				      "=0");
			StepFlowIndex.certify++;
		} else {
			if (TIMER_getMsecDiff(startEventTimeMsec) >= AT_CMD_LWM2M_TIMEOUT) {
				OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
				StepFlowIndex.certify = END_STEP_FLOW_INDEX;
			} else {
				OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_PROCESS,
						     MODEM_EVENT_INTERVAL);
			}
		}
	} break;

	case 2: {
		startEventTimeMsec = TIMER_getMsec();
		OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
		StepFlowIndex.certify++;
	} break;

	case 3: {
		if (modemCtx.lwm2m.regFinish && modemCtx.lwm2m.obsObj16241) {
			OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
			StepFlowIndex.certify++;
		} else {
			if (TIMER_getMsecDiff(startEventTimeMsec) >= AT_CMD_LWM2M_TIMEOUT) {
				OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
				StepFlowIndex.certify = END_STEP_FLOW_INDEX;
			} else {
				OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_PROCESS,
						     MODEM_EVENT_INTERVAL);
			}
		}
	} break;

	case END_STEP_FLOW_INDEX:
	default: {
		if (!modemCtx.lwm2m.bsFinish) {
			printf_ts("PF : Fail to bootstrap\n");
			MODEM_stop(FAIL);
		} else {
			if (!modemCtx.lwm2m.regFinish || !modemCtx.lwm2m.obsObj16241) {
				printf_ts("PF : Fail to register\n");
				MODEM_stop(FAIL);
			} else {
				nextStep = MODEM_STEP_FOTA;
			}
		}
	} break;
	}

	return nextStep;
}

/**
 * @brief FOTA step operator
 *
 * @return ModemStep_t    return next step
 */
static ModemStep_t fota()
{
	ASSERT_PRINT(modemCtx.step == MODEM_STEP_FOTA, "step is invalid \n");

	ModemStep_t nextStep = modemCtx.step;
	modemCtx.errCode = MODEM_ERROR_NONE;

	if (modemCtx.stepReset) {
		StepFlowIndex.fota = 0;
	}

	static uint32 startEventTimeMsec = 0;
	switch (StepFlowIndex.fota) {
	case 0: {
		startEventTimeMsec = TIMER_getMsec();
		OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
		StepFlowIndex.fota++;
	} break;

	case 1: {
		if (modemCtx.lwm2m.fotaDownReq) {
			sendAtCommand(AT_CMD_LWM2M_TIMEOUT, AT_CMD_LWM2M_RETRY, &AtCmdSetFOTA,
				      "=2");
			StepFlowIndex.fota++;
		} else {
			if (TIMER_getMsecDiff(startEventTimeMsec) >= MODEM_FW_DOWN_REQ_TIMEOUT) {
				OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
				StepFlowIndex.fota = END_STEP_FLOW_INDEX;
			} else {
				OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_PROCESS,
						     MODEM_EVENT_INTERVAL);
			}
		}
	} break;

	case 2: {
		startEventTimeMsec = TIMER_getMsec();
		OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
		StepFlowIndex.fota++;
	} break;

	case 3: {
		if (modemCtx.lwm2m.fotaUpgradeReq) {
			sendAtCommand(AT_CMD_LWM2M_TIMEOUT, AT_CMD_LWM2M_RETRY, &AtCmdSetFOTA,
				      "=4");
			StepFlowIndex.fota++;
		} else {
			if (TIMER_getMsecDiff(startEventTimeMsec) >= MODEM_FW_DOWNLOAD_TIMEOUT) {
				OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
				StepFlowIndex.fota = END_STEP_FLOW_INDEX;
			} else {
				OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_PROCESS,
						     MODEM_EVENT_INTERVAL);
			}
		}
	} break;

	case 4: {
		startEventTimeMsec = TIMER_getMsec();
		OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
		StepFlowIndex.fota++;
	} break;

	case 5: {
		if (modemCtx.lwm2m.fotaFinish) {
			OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
			StepFlowIndex.fota++;
		} else {
			if (TIMER_getMsecDiff(startEventTimeMsec) >= MODEM_FW_UPGRADE_TIMEOUT) {
				OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
				StepFlowIndex.fota = END_STEP_FLOW_INDEX;
			} else {
				OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_PROCESS,
						     MODEM_EVENT_INTERVAL);
			}
		}
	} break;

	case 6: {
		startEventTimeMsec = TIMER_getMsec();
		OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
		StepFlowIndex.fota++;
	} break;

	case 7: {
		// FOTA 완료 후 완료 확인을 위한 재인증을 대기한다(1분)
		// 재인증 성공실패 여부와 관계없이 단말기 입장에서 FOTA는 성공한 것이므로
		// 성공여부를 판단하지 않는다.
		if (TIMER_getMsecDiff(startEventTimeMsec) >= MODEM_FOTA_COMPLETE_WAIT) {
			OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
			StepFlowIndex.fota = END_STEP_FLOW_INDEX;
		} else {
			OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_PROCESS,
					     MODEM_EVENT_INTERVAL);
		}
	} break;

	case END_STEP_FLOW_INDEX:
	default: {
		if (!modemCtx.lwm2m.fotaDownReq) {
			printf_ts("PF : Fail to enter PSM during No FOTA download request\n");
		} else {
			if (!modemCtx.lwm2m.fotaUpgradeReq) {
				printf_ts("PF : FOTA upgrade req timeout.\n");
			} else {
				if (!modemCtx.lwm2m.fotaFinish) {
					printf_ts("PF : FOTA complete timeout.\n");
				} else {
					// FOTA 완료 후 MODEM 초기화 재수행을 위해 init flag 초기화
					conf.isModemInit = FALSE;
					FLASH_saveConfigInfo(&conf);

					modemCtx.proc.runFota = 0;
				}
			}
		}
		nextStep = MODEM_STEP_DETACH_NW;
	} break;
	}

	return nextStep;
}

/**
 * @brief Detach step operator
 *
 * @return ModemStep_t    return next step
 */
static ModemStep_t detachNw()
{
	ASSERT_PRINT(modemCtx.step == MODEM_STEP_DETACH_NW, "step is invalid \n");

	ModemStep_t nextStep = modemCtx.step;
	modemCtx.errCode = MODEM_ERROR_AT_CMD_NO_RESP;

	if (modemCtx.stepReset) {
		StepFlowIndex.detach = 0;
	}

	static uint32 startEventTimeMsec = 0;
	switch (StepFlowIndex.detach) {
	case 0: {
		// FOTA 완료 시 기존 인증절차도 초기화 되므로 De-register를 할 필요가 없음.
		// 다만, 단말 F/W에서 진행여부를 판단하는 Flag이므로 초기화는 안함.
		if (modemCtx.lwm2m.regFinish && !modemCtx.lwm2m.fotaFinish) {
			sendNoRespAtCommand(AT_CMD_COMM_TIMEOUT, &AtCmdRunRegister, "=1");
		}
		sendAtCommand(AT_CMD_COMM_TIMEOUT, AT_CMD_COMM_DETACH_RETRY, &AtCmdDetachNw, "=0");

		startEventTimeMsec = TIMER_getMsec();
		StepFlowIndex.detach++;
	} break;

	case 1: {
		// wait detach time
		if (TIMER_getMsecDiff(startEventTimeMsec) >= AT_CMD_COMM_TIMEOUT) {
			OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
			StepFlowIndex.detach++;
		} else {
			OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_PROCESS,
					     MODEM_EVENT_INTERVAL);
		}
	} break;

	default: {
		nextStep = MODEM_STEP_IDLE;
	} break;
	}

	return nextStep;
}

/**
 * @brief RETRY step operator
 *
 * @return ModemStep_t    return next step
 */
static ModemStep_t retry()
{
	ASSERT_PRINT(modemCtx.step == MODEM_STEP_RETRY, "step is invalid \n");
	ASSERT_PRINT(modemCtx.retryCount > 0, "retry count is not increased \n");
	ASSERT_PRINT(modemCtx.retryStep != MODEM_STEP_UNKNOWN, "MODEM retry step is not set \n");
	ASSERT_PRINT(modemCtx.retryStep != MODEM_STEP_IDLE, "MODEM retry step is initialized \n");

	// step 종료 전 까지 현재 step을 next step 으로 return.
	ModemStep_t nextStep = modemCtx.step;
	modemCtx.errCode = MODEM_ERROR_AT_CMD_NO_RESP;

	if (modemCtx.stepReset) {
		StepFlowIndex.retry = 0;
	}

	static uint32 startEventTimeMsec = 0;
	switch (StepFlowIndex.retry) {
	case 0: {
		if (modemCtx.status.cellreg == MODEM_CELLREG_ATTACHED) {
			if (modemCtx.lwm2m.regFinish) {
				sendNoRespAtCommand(AT_CMD_COMM_TIMEOUT, &AtCmdRunRegister, "=1");
			}
			sendAtCommand(AT_CMD_COMM_TIMEOUT, AT_CMD_COMM_DETACH_RETRY, &AtCmdDetachNw,
				      "=0");
		} else {
			OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
		}
		StepFlowIndex.retry++;
	} break;

	case 1: {
		if (modemCtx.proc.hwRstRetry) {
			MODEM_turnOff();
			MODEM_turnOn();
			startEventTimeMsec = TIMER_getMsec();
			OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
			StepFlowIndex.retry++;
		} else {
			sendAtCommand(AT_CMD_RESET_TIMEOUT, AT_CMD_RESET_RETRY, &AtCmdSwReset,
				      NULL);
			StepFlowIndex.retry = END_STEP_FLOW_INDEX;
		}
	} break;

	case 2: {
		if (TIMER_getMsecDiff(startEventTimeMsec) >= MODEM_BOOTUP_INTERVAL) {
			OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
			StepFlowIndex.retry++;
		} else {
			OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_PROCESS,
					     MODEM_EVENT_INTERVAL);
		}
	} break;

	case END_STEP_FLOW_INDEX:
	default: {
		if (modemCtx.proc.hwRstRetry) {
			// H/W reset 시 Idle 바로 다음 Step인 BIP 부터 수행.
			nextStep = MODEM_STEP_BIP;
		} else {
			if (modemCtx.retryStep == MODEM_STEP_BIP) {
				nextStep = MODEM_STEP_BIP;
			} else {
				nextStep = MODEM_STEP_ATTACH_NW;
			}
		}
	} break;
	}

	return nextStep;
}

BOOL MODEM_generateCseID(const char *serviceCode, const char *ctnStr, const char *iccidStr,
			 char *buffer, int size)
{
	if (ctnStr == NULL || iccidStr == NULL || serviceCode == NULL) {
		return FALSE;
	}

	if (strlen(serviceCode) != LEN_MODEM_SERVICE_CODE) {
		return FALSE;
	}

	char *cseid = getEndPointName(serviceCode, ctnStr, iccidStr);
	if (buffer != NULL && size > 0) {
		memcpy(buffer, cseid, size);
	} else {
		memcpy(modem.epName, cseid, sizeof(modem.epName));
	}

	return TRUE;
}

void MODEM_detach()
{
/*
     * 외부 Interrupt에 의한 단말기 Reboot전 Detach 및 Deregister 이므로
     * 서버/기지국 응답을 대기하지 않고 일정 Delay 후 종료한다.
     * 해당 동작은 UDP 기반이거나 예외처리 시 허용되는 동작이기 때문에 가능하다.
     */
#define DEREGISTER_DELAY_MS 5000
#define DETACH_DELAY_MS 5000

	if (modemCtx.lwm2m.regFinish) {
		// De-Register 전송.
		sendNoRespAtCommand(DEREGISTER_DELAY_MS, &AtCmdRunRegister, "=1");
	}
	// Detach 전송.
	sendNoRespAtCommand(DETACH_DELAY_MS, &AtCmdDetachNw, "=0");
}

BOOL MODEM_process()
{
	OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT);

	ModemStep_t nextStep = MODEM_STEP_UNKNOWN;
	switch (modemCtx.step) {
	case MODEM_STEP_IDLE:
		nextStep = idle();
		break;
	case MODEM_STEP_BIP:
		nextStep = bip();
		break;
	case MODEM_STEP_INIT:
		nextStep = init();
		break;
	case MODEM_STEP_ATTACH_NW:
		nextStep = attachNw();
		break;
	case MODEM_STEP_UPDATE_QA:
		nextStep = updateQa();
		break;
	case MODEM_STEP_TRANSFER:
		nextStep = transfer();
		break;
	case MODEM_STEP_CERTIFY:
		nextStep = certify();
		break;
	case MODEM_STEP_FOTA:
		nextStep = fota();
		break;
	case MODEM_STEP_DETACH_NW:
		nextStep = detachNw();
		break;
	case MODEM_STEP_RETRY:
		nextStep = retry();
		break;
	default:
		return FALSE;
	}

	ASSERT_PRINT(nextStep != MODEM_STEP_UNKNOWN, "next step is not set when step finish");
	// 동작 중간에 stop된 경우를 고려.
	if (modemCtx.step != nextStep && !modemCtx.status.stop) {
#if defined(DEBUG)
		printf_ts("MODEM : current(%s) next(%s) busy(%d) stop(%d) error(%d) cellreg(%d)\n",
			  MODEM_STEP_STRING(modemCtx.step), MODEM_STEP_STRING(nextStep),
			  modemCtx.status.busy, modemCtx.status.stop, modemCtx.errCode,
			  modemCtx.status.cellreg);
#endif

		// 완료된 현재 step과 retry step이 동일한 경우
		// retry 동작이 정상적으로 수행된 것을 의미하므로 retry 관련 context를 초기화.
		if (modemCtx.step == modemCtx.retryStep) {
			modemCtx.retryCount = 0;
			modemCtx.retryStep = MODEM_STEP_UNKNOWN;
		}

		modemCtx.step = nextStep;
		// step이 변경될 경우 해당 step을 최초부터 시작하기 위해 reset flag를 설정
		modemCtx.stepReset = TRUE;

		// If next step is IDLE and retry step is not set, process finish end success.
		if (modemCtx.step == MODEM_STEP_IDLE) {
			MODEM_stop(SUCCESS);
		} else {
			OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_PROCESS);
		}
	} else {
		modemCtx.stepReset = FALSE;
	}

	return TRUE;
}

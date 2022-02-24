#include <ctype.h>
#include <time.h>

#include "app.h"
#include "common_header.h"
#include "MSP430FlashUtil.h"

#include "osal_Timer.h"
#include "check_meter_misc.h"
#include "uart.h"
#include "shell.h"
#include "test.h"
#include "modem.h"
#include "mTest.h"

#if (defined(NBIOT_LG_TYPE) || defined(NBIOT_BASE_TYPE))
#include "uuid.h"
#endif

#if LORA_DEVICE
#define LEN_TEST_LORA_APPEUI 16
#define LEN_TEST_LORA_DEVEUI 16
#define LEN_TEST_LORA_APPKEY 32
#else // NBIOT_DEVICE
#define LEN_TEST_NBIOT_IMEI 15
#define LEN_TEST_NBIOT_IMSI 15
#define LEN_TEST_NBIOT_ICCID 19
#endif

struct {
	// will be set '1' when reading success MODEM booting message
	BOOL isAlive;
	// will be set '1' when want to show MODEM response
	BOOL isPrintOn;
	// 대다수의 AT COMMAND에 대해 적절한 응답이 올 경우 '1'로 설정.
	// 다만, 일부 AT COMMAND에 대해 처리가 안될 수 있으므로 사전 동작확인이 필요.
	BOOL isAcked;
	BOOL isError;
#if LORA_DEVICE
	char appEUI[LEN_TEST_LORA_APPEUI + 1]; // +1 wii be set 0. '0' is for string print.
	char devEUI[LEN_TEST_LORA_DEVEUI + 1]; // +1 wii be set 0. '0' is for string print.
	char appKey[LEN_TEST_LORA_APPKEY + 1]; // +1 wii be set 0. '0' is for string print.
#else // NBIOT_DEVICE
	BOOL isBipOk;
	BOOL isBootup;
	uint32 bootupMsec;
	char imei[LEN_TEST_NBIOT_IMEI + 1]; // +1 will be set 0. '0' is for string print.
	char imsi[LEN_TEST_NBIOT_IMSI + 1]; // +1 will be set 0. '0' is for string print.
	char iccid[LEN_TEST_NBIOT_ICCID + 1]; // +1 will be set 0. '0' is for string print.
#endif
} modemTestCtx;

/*
 ***************************************************************
    Modem common test functions
 ***************************************************************
 */

static void waitModem(int ms)
{
	int count = ms / 100;
	if (count == 0)
		count = 1;

	uint32 startMs = TIMER_getMsec();
	do {
		TEST_runTasks();
	} while (TIMER_getMsecDiff(startMs) <= ms);
}

static BOOL waitModemReady(int waitSec)
{
#if (LORA_DEVICE && defined(LORA_SKT_TYPE))
#define MODEM_CHECK_AT_CMD "AT+FWI"
#elif (LORA_DEVICE && defined(LORA_AS923_TYPE))
#define MODEM_CHECK_AT_CMD "AT+ADR"
#else // NBIOT_DEVICE
#define MODEM_CHECK_AT_CMD "AT"
#endif

	if (modemTestCtx.isAlive) {
		return TRUE;
	}

	if (waitSec > 100) {
		waitSec = 100;
	}

	uint32 sendMs = 0;
	uint32 waitMs = waitSec * 1000;
	uint32 startMs = TIMER_getMsec();
	do {
		if (TIMER_getMsecDiff(sendMs) >= 1500) {
			sendMs = TIMER_getMsec();
			MODEM_write(MODEM_CHECK_AT_CMD);
		}
		TEST_runTasks();
		if (modemTestCtx.isAlive) {
			return TRUE;
		}
	} while (TIMER_getMsecDiff(startMs) <= waitMs);

	return FALSE;
}

static BOOL waitModemAck(int waitSec)
{
	if (waitSec > 100) {
		waitSec = 100;
	}

	uint32 waitMs = waitSec * 1000;
	uint32 startMs = TIMER_getMsec();
	do {
		TEST_runTasks();
		if (modemTestCtx.isAcked) {
			return TRUE;
		}
		if (modemTestCtx.isError) {
			return FALSE;
		}
	} while (TIMER_getMsecDiff(startMs) <= waitMs);

	return FALSE;
}

#if NBIOT_DEVICE
static BOOL waitModemBipOk(int waitSec)
{
#define MODEM_BIP_CHECK_AT_CMD "AT+MBIPST?"

	if (modemTestCtx.isBipOk) {
		return TRUE;
	}

	if (waitSec > 100) {
		waitSec = 100;
	}

	uint32 sendMs = 0;
	uint32 waitMs = waitSec * 1000;
	uint32 startMs = TIMER_getMsec();
	do {
		if (TIMER_getMsecDiff(sendMs) >= 1500) {
			sendMs = TIMER_getMsec();
			MODEM_write(MODEM_BIP_CHECK_AT_CMD);
		}
		TEST_runTasks();
		if (modemTestCtx.isBipOk) {
			return TRUE;
		}
	} while (TIMER_getMsecDiff(startMs) <= waitMs);

	return FALSE;
}
#endif

static void sendToModem(char *p)
{
	// If modem ready is checked fail, do not send AT command to modem
	if (waitModemReady(5) == FALSE) {
		printf("Modem is not alive.\n");
		return;
	}

	modemTestCtx.isAcked = FALSE; // 이전 AT command에 대한 Ack 수신 Flag 초기화
	modemTestCtx.isError = FALSE; // 이전 AT command에 대한 Error 수신 Flag 초기화
	MODEM_write(p);
}

#if NBIOT_DEVICE
static void resetModem()
{
	// 모뎀 S/W 리셋
	sendToModem("AT+NRB");
	// 부팅 재확인 및 CFUN=0 재실행을 위해 Flag 초기화.
	modemTestCtx.isBootup = FALSE;
	modemTestCtx.isAlive = FALSE;
	modemTestCtx.isBipOk = FALSE;
}
#endif

static void printAtCmdList()
{
	printf("\n");
	printf(TP_ANSI_FG_GREEN);
	printf("Mainly Used AT command List\n");
#if LORA_DEVICE
#if defined(LORA_AS923_TYPE)
	printf(" AT+AK   : Read Application Key \n");
	printf(" AT+DEUI : Read Device EUI \n");
	printf(" AT+AEUI : Read Application EUI \n");
	printf(" AT+POW  : Get/Set Tx Power (Range: 0~6) \n");
	printf(" AT+REG  : Get/Set region \n");
	printf("     (0:AS923, 1:AU915, 2:CN470, 3:CN779, 4:EU433, 5:EU868, 6:KR920, 7:IN865, 8:US915, 9:US915_HYBRID) \n");
	printf(" AT+ADR  : Get Current Adaptive Data Rate\n");
	printf(" AT+CLS  : Get Current Select Class\n");
	printf(" AT+GCFG : Get Configuration \n");
	printf(" AT+LOG  : Set Log Message (0:Disable, 1:Enable) \n");
	printf(" AT+CFM  : Set Confirm Message (0:Disable, 1:Enable) \n");
	printf(" AT+SEND : SEND Message \n");
	printf("     Eample) 'AT+SEND 02112233' Port:02, Data:112233 \n");
	printf(" AT+RST  : Reset the LoRa Module \n");
#else
	printf(" AT+AK   : Read Application Key \n");
	printf(" AT+DEUI : Read Device EUI \n");
	printf(" AT+AEUI : Read Application EUI \n");
	printf(" AT+SIG  : Get Latest SNR and RSSI Value \n");
	printf(" AT+CH   : Get Current Tx Channel \n");
	printf(" AT+ADR  : Get Current Adaptive Data Rate\n");
	printf(" AT+CLS  : Get Current Select Class\n");
	printf(" AT+FWI  : Get Firmware version \n");
	printf(" AT+GCFG : Get Configuration \n");
	printf(" AT+DUTC : Get duty cycle time\n");
	printf(" AT+LOG  : Set Log Message (0:Disable, 1:Enable) \n");
	printf(" AT+CFM  : Set Confirm Message (0:Disable, 1:Enable) \n");
	printf(" AT+CHTX : Set Channel & TX power\n");
	printf("     Eample) 'AT+CHTX 2701' CH:27, Power index:1\n");
	printf(" AT+SEND : SEND Message \n");
	printf("     Eample) 'AT+SEND 02112233' Port:02, Data:112233 \n");
	printf(" AT+RST  : Reset the LoRa Module \n");
#endif // defined(LORA_AS923_TYPE)
#else // NBIOT_DEVICE
	printf(" AT+CEREG?   : Get Current Attach Status (0:Nothing, 1:OK, 2:Running, 3:Reject)\n");
	printf("               Eample) '+CEREG:x,N' 'N' is status\n");
	printf(" AT+MBIPST?  : Get Current BIP Status (0:OK, 1:Running)\n");
	printf(" AT+CFUN?    : Get Current Modem Fuctionality (0:Disable, 1:All Enable) \n");
	printf(" AT+NCONFIG? : Get Modem Setting \n");
	printf(" AT+CMEE?    : Get Termination Error Code (0:None, Others:Error)\n");
	printf(" AT+CEER     : Get Extended Error\n");
	printf(" AT+NUESTATS : Get Current Signal Quality \n");
	printf(" AT+CGMR     : Get Firmware version \n");
	printf(" AT+CSGN=1   : Read IMEI \n");
	printf(" AT+CIMI     : Read IMSI \n");
	printf(" AT+NCCID    : Read ICCID \n");
	printf(" AT+NRB      : Reset the NB-IoT Module\n");
#endif
	printf(TP_ANSI_RESET);
	printf("\n");
}

#if LORA_DEVICE

static void parseLoraModemResp(char *pStr)
{
	char *p = NULL;

#if defined(LORA_AS923_TYPE)
#define PATTERN_AEUI "+AEUI="
#else
#define PATTERN_AEUI "APPLICATION EUI : "
#endif
	p = strstr(pStr, PATTERN_AEUI);
	if (p) {
		p += strlen(PATTERN_AEUI);
		int nDigit = 0;
		while (nDigit < LEN_TEST_LORA_APPEUI) {
			if (isxdigit(*p)) {
				modemTestCtx.appEUI[nDigit++] = *p;
			}
			p++;
		}
	}

#if defined(LORA_AS923_TYPE)
#define PATTERN_DEUI "+DEUI="
#else
#define PATTERN_DEUI "Device EUI : "
#endif
	p = strstr(pStr, PATTERN_DEUI);
	if (p) {
		p += strlen(PATTERN_DEUI);
		int nDigit = 0;
		while (nDigit < LEN_TEST_LORA_DEVEUI) {
			if (isxdigit(*p)) {
				modemTestCtx.devEUI[nDigit++] = *p;
			}
			p++;
		}
	}

#if defined(LORA_AS923_TYPE)
#define PATTERN_AKEY "+AK="
#else
#define PATTERN_AKEY "Application Key : "
#endif
	p = strstr(pStr, PATTERN_AKEY);
	if (p) {
		p += strlen(PATTERN_AKEY);
		int nDigit = 0;
		while (nDigit < LEN_TEST_LORA_APPKEY) {
			if (isxdigit(*p)) {
				modemTestCtx.appKey[nDigit++] = *p;
			}
			p++;
		}
	}

	// Check default AT command response pattern
#if defined(LORA_AS923_TYPE)
#define PATTERN_ACK_OK "OK"
#else
#define PATTERN_ACK_OK "- "
#endif
	if (p = strstr(pStr, PATTERN_ACK_OK)) {
		modemTestCtx.isAcked = TRUE;
	}
}

#else // NBIOT_DEVICE

static void parseNbiotModemResp(char *pStr)
{
	char *p = NULL;

#define PATTERN_IMEI "+CGSN:"
	if (p = strstr(pStr, PATTERN_IMEI)) {
		p += strlen(PATTERN_IMEI);
		for (int idx = 0; idx < LEN_TEST_NBIOT_IMEI; idx++) {
			if (*p >= '0' && *p <= '9') {
				modemTestCtx.imei[idx] = *p;
			} else {
				// 값이 연속된 15자리 숫자값이 아닌 경우 실패로 간주해야 한다.
				memset(modemTestCtx.imei, 0x00, sizeof(modemTestCtx.imei));
				modemTestCtx.imei[0] = 'E';
				modemTestCtx.imei[1] = 'R';
				modemTestCtx.imei[2] = 'R';
				break;
			}
			p++;
		}
	}

#define PATTERN_IMSI "450"
	if (p = strstr(pStr, PATTERN_IMSI)) {
		// CTN의 경우 PREFIX가 없어 데이터정합성 유무를 정확히 알기 힘듬.
		// 위 이유로 IMEI, ICCID와 다르게 ERROR처리를 생략한다.
		BOOL isCtn = TRUE;
		for (int idx = 0; idx < LEN_TEST_NBIOT_IMSI; idx++) {
			if (!isdigit(*(p + idx))) {
				isCtn = FALSE;
			}
		}
		if (isCtn) {
			for (int idx = 0; idx < LEN_TEST_NBIOT_IMSI; idx++) {
				modemTestCtx.imsi[idx] = *(p + idx);
			}
		}
	}

#define PATTERN_ICCID "+NCCID:"
	if (p = strstr(pStr, PATTERN_ICCID)) {
		p += strlen(PATTERN_ICCID);
		for (int idx = 0; idx < LEN_TEST_NBIOT_ICCID; idx++) {
			if (*p >= '0' && *p <= '9') {
				modemTestCtx.iccid[idx] = *p;
			} else {
				// 값이 연속된 19자리 숫자값이 아닌 경우 실패로 간주해야 한다.
				memset(modemTestCtx.iccid, 0x00, sizeof(modemTestCtx.iccid));
				modemTestCtx.iccid[0] = 'E';
				modemTestCtx.iccid[1] = 'R';
				modemTestCtx.iccid[2] = 'R';
				break;
			}
			p++;
		}
	}

// Check default AT command response pattern
#define PATTERN_ACK_OK "OK"
	if (p = strstr(pStr, PATTERN_ACK_OK)) {
		modemTestCtx.isAcked = TRUE;
	}

// Check error AT command response pattern
#define PATTERN_ACK_ERROR "ERROR"
	if (p = strstr(pStr, PATTERN_ACK_ERROR)) {
		modemTestCtx.isError = TRUE;
	}
}

#endif

void TEST_initModem(int taskId)
{
	memset(&modemTestCtx, 0, sizeof(modemTestCtx));
	MODEM_initialization();
	MODEM_turnOn();
	MODEM_enable();
}

void TEST_modem()
{
	printf("===========================================\n");
	printf("    AT command direct input mode \n");
	printf("    (Use 'exit' to exit mode)  \n");
	printf("    (Use  '?'   to show command list) \n");
	printf("===========================================\n");

	modemTestCtx.isPrintOn = TRUE;

	// Console을 통해 Shell command를 받아 처리하므로
	// 최대 길이는 Shell command buffer 최대 길이에 종속 된다. (+1 is for string end)
	char shellCmdBuf[MAX_COLUMN + 1];
	while (1) {
		while (SHELL_gets(shellCmdBuf) <= 0)
			;

		if (strcmp(shellCmdBuf, "exit") == 0) {
			break;
		} else if (strcmp(shellCmdBuf, "?") == 0) {
			printAtCmdList();
		} else {
			sendToModem(shellCmdBuf);
		}
	}

	modemTestCtx.isPrintOn = FALSE;
#if NBIOT_DEVICE
	resetModem();
#endif
}

void TEST_modemResponse()
{
	char buf[MODEM_RX_BUF_LEN + 1];
	memset(buf, 0, MODEM_RX_BUF_LEN + 1);
	int len = UART_receive(UART_A2, buf, MODEM_RX_BUF_LEN);
	if (modemTestCtx.isPrintOn &&
	    len > 1) { // 무조건 '\0'이 할당되므로 메세지가 없더라도 len은 0보다 큼.
		printf(TP_ANSI_FG_RED);
		PRINT_string(buf, len);
		printf(TP_ANSI_RESET);
	}

	// semco: REBOOT_CAUSE, solu-m: VERSION(debug=0), LOG Message(debug=1)
	if (!modemTestCtx.isAlive) {
#if LORA_DEVICE
		if (strstr(buf, "VERSION") || strstr(buf, "LOG Message")) {
			modemTestCtx.isAlive = TRUE;
		}
#else // NBIOT_DEVICE                                                          \
	// 기타 문제로 모듈이 지속적으로 리셋될 경우를 고려하여                \
	// REBOOT이 발생될 경우 Flag 및 시간값 초기화.
		if (strstr(buf, "REBOOT_CAUSE_")) {
			modemTestCtx.isBootup = TRUE;
			modemTestCtx.bootupMsec = TIMER_getMsec();
		}

		if (modemTestCtx.isBootup) {
			// 리셋 후 부팅완료까지 5초가 필요하지만 'REBOOT' 메세지는
			// 약 4초 후 발생하므로 실제 부팅 완료 까지 추가딜레이가 필요.
			if (TIMER_getMsecDiff(modemTestCtx.bootupMsec) >= 1000) {
				modemTestCtx.isAlive = TRUE;
#if defined(NBIOT_MODEM_BC95G)
				// BC95G는 BIP 동작 중에도 정상적으로 동작이 가능하므로 BIP체크 생략.
				modemTestCtx.isBipOk = TRUE;
#endif
				// Alive 설정 후 AT command 전송.
				sendToModem("AT+CFUN=0");
			}
		}
		OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_RX, 1000);
#endif
	} else {
#if LORA_DEVICE
		parseLoraModemResp(buf);
#else // NBIOT_DEVICE
		parseNbiotModemResp(buf);
#endif
	}
}

void TEST_pmodem()
{
#if LORA_DEVICE
	if (waitModemReady(1)) {
#else // NBIOT_DEVICE
	if (waitModemReady(1) && waitModemBipOk(5)) {
#endif
		printf("tmodem OK\n");
	} else {
		printf("tmodem ERR\n");
	}
}

/*
 ***************************************************************
    LoRa test functions
 ***************************************************************
 */
#if LORA_DEVICE

void TEST_prdeui()
{
	sendToModem("AT+DEUI");
	if (waitModemAck(2) == FALSE) {
		printf("trdeui ERR\n");
	} else {
		printf("trdeui %s\n", modemTestCtx.devEUI);
	}

	// save devEui in flash
	Config_t config;
	FLASH_readConfigInfo(&config);

	for (int i = 0; i < 8; i++) {
		config.devEui[i] =
			ascii2BCD(modemTestCtx.devEUI[2 * i + 0], modemTestCtx.devEUI[2 * i + 1]);
	}
	FLASH_saveConfigInfo(&config);
}

void TEST_pwdeui(char *deui)
{
	// By default,LoRa modem prohibit to write Device EUI
	printf("tdeui ERR\n");
}

void TEST_praeui()
{
	sendToModem("AT+AEUI");
	if (waitModemAck(2) == FALSE) {
		printf("traeui ERR\n");
	} else {
		printf("traeui %s\n", modemTestCtx.appEUI);
	}
}

void TEST_pwaeui(char *aeui)
{
	if (strlen(aeui) != LEN_TEST_LORA_APPEUI) {
		printf("traeui ERR\n");
		return;
	}

	// 9 is length of 'AT+AEUI ' and string end
	char cmd[LEN_TEST_LORA_APPEUI + 9] = "";
	snprintf(cmd, sizeof(cmd), "AT+AEUI %s", aeui);

	sendToModem(cmd);
	waitModem(500);
	sendToModem("AT+SCFG");
	waitModem(500);

	TEST_praeui();
}

void TEST_prakey()
{
	sendToModem("AT+AK");
	if (waitModemAck(2) == FALSE) {
		printf("trakey ERR\n");
	} else {
		printf("trakey %s\n", modemTestCtx.appKey);
	}
}

void TEST_pwakey(char *akey)
{
	if (strlen(akey) != LEN_TEST_LORA_APPKEY) {
		printf("trakey ERR\n");
		return;
	}

	// 7 is length of 'AT+AK ' and string end
	char cmd[LEN_TEST_LORA_APPKEY + 7] = "";
	snprintf(cmd, sizeof(cmd), "AT+AK %s", akey);

	sendToModem(cmd);
	waitModem(500);
	sendToModem("AT+SCFG");
	waitModem(500);

	TEST_prakey();
}

BOOL TEST_writeAppKeyUsingDEUI()
{
	sendToModem("AT+DEUI");
	waitModem(500);

	// 7 is length of 'AT+AK ' and string end
	char modemTx[LEN_TEST_LORA_APPKEY + 7];
	snprintf(modemTx, sizeof(modemTx), "AT+AK ");

	// Dev EUI를 AppKey로 변환 (Hex to ASCII)
	for (int i = 0; i < LEN_TEST_LORA_DEVEUI; i++) {
		snprintf(modemTx, sizeof(modemTx), "%s%x", modemTx, modemTestCtx.devEUI[i]);
	}

	sendToModem(modemTx);
	waitModem(500);
	// Read AppKey가 기대값과 동일한지 확인.
	if (memcmp(&modemTx[6], modemTestCtx.appKey, LEN_TEST_LORA_APPKEY) == 0) {
		// Write한 AppKey 저장.
		sendToModem("AT+SCFG");
		waitModem(500);
		return TRUE;
	}
	return FALSE;
}

#endif // LORA_DEVICE

/*
 ***************************************************************
    NB-IoT test functions
 ***************************************************************
 */
#if NBIOT_DEVICE

void TEST_pwserver(char *ip, int port, char *serviceCode)
{
	Config_t config;
	FLASH_readConfigInfo(&config);

	strcpy(config.serverIp, ip);
	config.serverPort = port;

#if (defined(NBIOT_LG_TYPE) || defined(NBIOT_BASE_TYPE))
	int sc_len = sizeof(config.serviceCode);
	strncpy(config.serviceCode, serviceCode, sc_len);
	// 배열 마지막은 string처리를 위해 '0'으로 설정.
	config.serviceCode[sc_len - 1] = 0;
#endif

	FLASH_saveConfigInfo(&config);

	TEST_prserver();
}

void TEST_prserver()
{
	Config_t config;
	FLASH_readConfigInfo(&config);
#if (defined(NBIOT_LG_TYPE) || defined(NBIOT_BASE_TYPE))
	printf("trserver %s %d %s\n", config.serverIp, config.serverPort, config.serviceCode);
#else
	printf("trserver %s %d NOTUSE\n", config.serverIp, config.serverPort);
#endif
}

void TEST_pwfota(char *ip, int port, int interval)
{
	Config_t config;
	FLASH_readConfigInfo(&config);

	strcpy(config.fotaIp, ip);
	config.fotaPort = port;
	config.fotaInterval = interval;

	FLASH_saveConfigInfo(&config);

	TEST_prfota();
}

void TEST_prfota()
{
	Config_t config;
	FLASH_readConfigInfo(&config);
	printf("trfota %s %d %d\n", config.fotaIp, config.fotaPort, config.fotaInterval);
}

void TEST_prnbid()
{
	BOOL isImeiOk = FALSE;
	BOOL isImsiOk = FALSE;
	BOOL isIccidOk = FALSE;

	if (!waitModemBipOk(1)) {
		// BIP 완료 전 "+CFUN=1"을 할경우 BIP가 진행되므로 완료 여부를 확인.
		printf("trnbid ERR ERR ERR ERR\n");
		return;
	}

	sendToModem("AT+CGSN=1");
	isImeiOk = waitModemAck(2);

	// USIM IMSI 및 ICCID 읽기의 경우 USIM이 없는 경우도 있으므로
	// 아래와 같이 '+CFUN=1'이 성공했을 때만 시도한다.
	sendToModem("AT+CFUN=1");
	if (waitModemAck(4)) {
		sendToModem("AT+CIMI");
		isImsiOk = waitModemAck(2);
		sendToModem("AT+NCCID");
		isIccidOk = waitModemAck(2);
	}

	Config_t config;
	FLASH_readConfigInfo(&config);

	if (isImeiOk && isImsiOk && isIccidOk) {
#if (defined(NBIOT_LG_TYPE) || defined(NBIOT_BASE_TYPE))
		char ctnStr[LEN_MODEM_CTN + 1];
		snprintf(ctnStr, sizeof(ctnStr), "0%s", &modemTestCtx.imsi[5]);

		char cseid[LEN_MODEM_EP_NAME + 1];
		if (MODEM_generateCseID(config.serviceCode, ctnStr, modemTestCtx.iccid, cseid,
					sizeof(cseid))) {
			printf("trnbid %s %s %s %s\n", modemTestCtx.imei, modemTestCtx.imsi, cseid,
			       modemTestCtx.iccid);
		} else {
			printf("trnbid %s %s ERR %s\n", modemTestCtx.imei, modemTestCtx.imsi,
			       modemTestCtx.iccid);
		}
#else
		printf("trnbid %s %s NOTUSE %s\n", modemTestCtx.imei, modemTestCtx.imsi,
		       modemTestCtx.iccid);
#endif
	} else {
		printf("trnbid %s %s ERR %s\n", (isImeiOk) ? modemTestCtx.imei : "ERR",
		       (isImsiOk) ? modemTestCtx.imsi : "ERR",
		       (isIccidOk) ? modemTestCtx.iccid : "ERR");
	}

	if (isImeiOk) { // IMEI 읽기 성공 시 Flash에 저장.
		for (int i = 0; i < 8; i++) {
			config.imei[i] = ascii2BCD(modemTestCtx.imei[2 * i + 0],
						   modemTestCtx.imei[2 * i + 1]);
		}
		config.imei[7] &= 0xf0;
		FLASH_saveConfigInfo(&config);
	}

	sendToModem("AT+CFUN=0");
	waitModem(500);
}

void TEST_pnbctx()
{
	if (!waitModemReady(2)) {
		// "테스트 준비중"을 확인 및 보고 해야하므로
		// 테스트 시작 전 모뎀 READY 여부를 확인.
		printf("tnbctx WAIT\n");
		return;
	}
	waitModem(500); // ready 후 시행하는 command 처리 대기시간.

	BOOL result = FALSE;
	// detach and full function disable
	sendToModem("AT+CFUN=0");
	result = waitModemAck(2);
	if (!result) {
		goto PNBCTX_FINISH;
	}

	// chage to Test Mode #1
	sendToModem("AT+NRDTEST=8,0000400213020000");
	result = waitModemAck(2);
	if (!result) {
		goto PNBCTX_FINISH;
	}

	// chage to Test Mode #2
	sendToModem("AT+NRDTEST=8,0100400213020000");
	result = waitModemAck(2);
	if (!result) {
		goto PNBCTX_FINISH;
	}

	// start 848.9MHz continuous Tx
	sendToModem(
		"AT+NRDTEST=240,"
		"030040021302e800E0D44735A02F9932590A0000000000000000000002000000005802000058020000580200000000000a00000080"
		"02000000000000c8000000000000000000000000000000000000000a000000000000000b0002000001000017000100000000000000"
		"0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
		"0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
		"0000000000000000000000000000000000011900100e000001010000");
	result = waitModemAck(2);
	if (!result) {
		goto PNBCTX_FINISH;
	}

PNBCTX_FINISH:
	if (result) {
		printf("tnbctx OK\n");
		while (1) {
			if (TEST_stop()) {
				break;
			}
			MISC_delayMs(1);
		}
	} else {
		printf("tnbctx ERR\n");
	}

	resetModem();
}

#endif // NBIOT_DEIVCE

// TODO : SKT 테스트 및  10초 주기 unconfirmed message 전송 기능. 추후 구현해야 함.
// #if (FOR_SKT_TEST && PERIODIC_TX)
//     LCD_init();
//     LCD_displayString("TEST");

//     modemTest.tx_printFlag = modemTest.rx_printFlag = 1;
//     send_to_modem("AT+ADR");

//     int bLoop = 1;
//     do {
//         while(bLoop) {
//             if(modemTest.provision) {
//                 break;
//             }
//             TEST_runTasks();
//             MISC_delayMs(100);
//             if(TEST_stop()) {
//                 bLoop = 0;
//                 break;
//             }
//         }

//         if(bLoop == 0) {
//             break;
//         }

//         MISC_delayMs(1000);
//         send_to_modem("AT+CFM 0");

//         int count = 0;
//         while(bLoop) {
//             uint32 ms = TIMER_getMsec();

//             char buf[0x10] = "";
//             snprintf(buf, 0x10, "%05d", ++count);
//             LCD_displayString(buf);

//             printf("### %d ###\n", count);

//             char command[0x100] = "AT+SEND 02";
//             for(int i = 0; i < 40; i++) {
//                 snprintf(command, 0x100, "%s%02x", command, i+1);
//             }
//             send_to_modem(command);

//             while(1) {
//                 if(TIMER_getMsecDiff(ms) >= (10 * 1000)) {
//                     break;
//                 }
//                 TEST_runTasks();
//                 MISC_delayMs(1);
//                 if(TEST_stop()) {
//                     bLoop = 0;
//                     break;
//                 }
//             }
//         }
//     } while(0);

//     modemTest.tx_printFlag = modemTest.rx_printFlag = 0;
//     send_to_modem("AT+LOG 0");
//     MISC_delayMs(100);
//     send_to_modem("AT+CFM 1");
// #endif


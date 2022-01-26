#include <string.h>
#include <stdlib.h>

#include "tdd.h"
#include "nbiotResponse.h"

extern Config_t conf;
extern Modem_t modem;

#ifdef TDD_TEST
#include <stdio.h>

#else

#include <msp430.h>
#include <ctype.h>
#include <time.h>

#include "uart.h"
#include "check_meter_misc.h"

#include "modem.h"
#include "osal_Timer.h"
#include "app.h"
#include "rtcAlarm.h"
#include "lcdDriver.h"
#include "message.h"

static int parse_imei(char *p)
{
	if (strlen(p) >= LEN_MODEM_IMEI_IMSI) {
		char *q = modem.imeiStr;
		int valid = 1;
		for (int i = 0; i < LEN_MODEM_IMEI_IMSI; i++) {
			if (*p >= '0' && *p <= '9') {
				*q++ = *p++;
			} else {
				valid = 0;
				break;
			}
		}

		if (valid) {
			*q = '\0';
		}

		return valid;
	} else {
		return 0;
	}
}

static int parse_imsi(char *p)
{
	int len = strlen(p);
	if (len >= LEN_MODEM_IMEI_IMSI) {
		int valid = 1;
		char *q = modem.imsiStr;
		for (int i = 0; i < LEN_MODEM_IMEI_IMSI; i++) {
			if (*p >= '0' && *p <= '9') {
				*q++ = *p++;
			} else {
				valid = 0;
				break;
			}
		}

		if (valid) {
			*q = '\0';
			if (strncmp(modem.imsiStr, IMSI_MCC_MNC_KT_1, strlen(IMSI_MCC_MNC_KT_1)) ==
				    0 ||
			    strncmp(modem.imsiStr, IMSI_MCC_MNC_KT_2, strlen(IMSI_MCC_MNC_KT_2)) ==
				    0) {
				modem.modemType = MODEM_TYPE_KT;
			} else if (strncmp(modem.imsiStr, IMSI_MCC_MNC_UPLUS,
					   strlen(IMSI_MCC_MNC_UPLUS)) == 0) {
				modem.modemType = MODEM_TYPE_UPLUS;
#define IMSI_UPLUS_VALID IMSI_MCC_MNC_UPLUS "12"
				if (strncmp(modem.imsiStr, IMSI_UPLUS_VALID,
					    strlen(IMSI_UPLUS_VALID)) != 0) {
					valid = 0;
				}
			} else {
				modem.modemType = MODEM_TYPE_UNKNOWN;
				valid = 0;
			}
		}

		return valid;
	} else {
		return 0;
	}
}

static int parse_iccid(char *p)
{
	int valid = 1;
	char *q = modem.iccidStr;
	for (int i = 0; i < LEN_MODEM_ICCID; i++) {
		if (*p >= '0' && *p <= '9') {
			*q++ = *p++;
		} else {
			valid = 0;
			break;
		}
	}

	if (valid) {
		*q = '\0';
	}

	return valid;
}

static int parse_ncdp(char *p)
{
	char addr[22];
	snprintf(addr, sizeof(addr), "%s,%d", conf.serverIp, conf.serverPort);

	int matched = 0;
	if (strncmp((const char *)p, (const char *)addr, strlen(addr)) != 0) {
		printf_ts("LWM2M : server address is not matched \n");
	} else {
		matched = 1;
	}

	return matched;
}

static int parse_lwm2m_server(char *p)
{
	char addr[22];
	snprintf(addr, sizeof(addr), "%s,%d", conf.serverIp, conf.serverPort);

	int matched = 0;
	if (strncmp((const char *)p, (const char *)addr, strlen(addr)) != 0) {
		printf_ts("LWM2M : server address is not matched \n");
	} else {
		matched = 1;
	}

	return matched;
}

static int parse_bs_param(char *p)
{
	int matched = 0;
	int len = LEN_MODEM_BS_PARAM;
	char str[LEN_MODEM_BS_PARAM];
	memcpy(str, p, len);

	char *pStr = str;
	for (; *pStr != '\0'; pStr++) { //종료 문자를 만날 때까지 반복
		if (*pStr == '\"') {
			strcpy(pStr, pStr + 1);
			len--;
			pStr--;
		}
		if (len-- <= 0)
			break;
	}

	if (strncmp((const char *)str, (const char *)modem.bsParam, strlen(modem.bsParam)) != 0) {
		printf_ts("LWM2M : bootstrap parameter is not matched\n", str);
	} else {
		matched = 1;
	}
	return matched;
}

static int parse_ep_name(char *p)
{
	int matched = 0;
	if (strncmp((const char *)p, (const char *)modem.epName, strlen(modem.epName)) != 0) {
		printf_ts("LWM2M : endpoint name is not matched \n");
	} else {
		matched = 1;
	}

	return matched;
}

static int parse_server_notify(char *p)
{
	int valid = 1, cerify = 0;
	if (*p == '0') {
		printf_ts("LWM2M : register finish \n");
		modemCtx.lwm2m.regFinish = cerify = 1;
		modemCtx.lwm2m.regDelete = 0;
	} else if (*p == '1') {
		printf_ts("LWM2M : register delete \n");
		modemCtx.lwm2m.regDelete = 1;
		modemCtx.lwm2m.regFinish = 0;
		modemCtx.lwm2m.obsObj10250 = 0;
		modemCtx.lwm2m.obsObj16241 = 0;
	} else if (*p == '2') {
		printf_ts("LWM2M : register update \n");
		modemCtx.lwm2m.regUpdate = cerify = 1;
	} else if (*p == '3') {
		printf_ts("LWM2M : observe object 10250 \n");
		modemCtx.lwm2m.obsObj10250 = 1;
	} else if (*p == '4') {
		printf_ts("LWM2M : boostrap finish \n");
		modemCtx.lwm2m.bsFinish = 1;
	} else if (*p == '5') {
		printf_ts("LWM2M : observe object 5/0/3 \n");
		modemCtx.lwm2m.obsObj503 = 1;
	} else if (*p == '6') {
		printf_ts("LWM2M : FOTA downlink request \n");
		modemCtx.lwm2m.fotaDownReq = 1;
	} else if (*p == '7') {
		printf_ts("LWM2M : FOTA upgrade request \n");
		modemCtx.lwm2m.fotaUpgradeReq = 1;
	} else if (*p == '8') {
		printf_ts("LWM2M : observe object 16241 \n");
		modemCtx.lwm2m.obsObj16241 = 1;
	} else if (*p == '9') {
		printf_ts("LWM2M : cancel object 10250 \n");
		modemCtx.lwm2m.cxlObj10250 = 1;
	} else {
		valid = 0;
	}

	if (cerify) {
		RTC_read(&modem.lastCertifyTime);
	}

	return valid;
}

static int parse_udp_downlink(char *p)
{
	// 0,211.211.42.30,19001,3,123456,0
	// socket, ip, portno, len, data, more

	int valid = 0;
	char ipAddr[SERVER_IP_STR_LEN];

	do {
		// ip address Search (Downlink 응답이 아닌것을 필터링)
		strncpy(ipAddr, conf.serverIp, SERVER_IP_STR_LEN);
		if ((p = strstr(p, ipAddr)) == NULL) {
			break;
		}
		p = p - 2;

		// socket number check
		int socket = atoi(p);
		if (modemCtx.socket != socket) {
			printf_ts("DL : socket mismatched (my : %d, current : %d)\n",
				  modemCtx.socket, socket);
			break;
		}

		// skip first comma
		if ((p = strstr(p, ",")) == NULL) {
			break;
		}
		p++;

		// ip address
		char ip[SERVER_IP_STR_LEN] = "";

		char *q = strstr(p, ",");
		if (q == NULL) {
			break;
		}

		memcpy(ip, p, (int)(q - p));
		ip[(int)(q - p)] = '\0';

		// skip second comma
		p = q + 1;

		//port number
		int port = atoi(p);

		// skip third comma
		if ((p = strstr(p, ",")) == NULL) {
			break;
		}
		p++;

		// rx len, update real DL data length
		int rxLen = atoi(p);

		// skip fourth comma
		if ((p = strstr(p, ",")) == NULL) {
			break;
		}
		p++;

		// rx data
		for (int i = 0; i < rxLen; i++) {
			modemComm.dlData[i] = ascii2BCD(*(p + i * 2 + 0), *(p + i * 2 + 1));
		}
		modemComm.dlDataLen = rxLen;

		// skip fifth comma
		if ((p = strstr(p, ",")) == NULL) {
			break;
		}
		p++;

		// more field
		int more = atoi(p);

		valid = 1;
	} while (0);

	return valid;
}

static int parse_pf_downlink(char *p)
{
	// 6,0DEBD17AC7AD
	// len,data

	int valid = 0;
	do {
		// socket number
		int len = atoi(p);
		if (len > LEN_MAX_DL_DATA) {
			printf_ts("DL : data is too long (max : %d, current :%d)\n",
				  LEN_MAX_DL_DATA, len);
			break;
		} else {
			printf_ts("DL : data len (current :%d)\n", len);
		}

		// skip first comma
		if ((p = strstr(p, ",")) == NULL)
			break;
		p++;

		for (int i = 0; i < len; i++) {
			modemComm.dlData[i] = ascii2BCD(*(p + i * 2 + 0), *(p + i * 2 + 1));
		}
		modemComm.dlDataLen = len;

		modemCtx.waitDl = FALSE;
		valid = 1;
	} while (0);

	return valid;
}

static int parse_cclk(char *p)
{
	Date_t date;

	date.year = atoi(p) + 2000;
	date.mon = atoi(p + 3);
	date.day = atoi(p + 6);
	date.hour = atoi(p + 9);
	date.min = atoi(p + 12);
	date.sec = atoi(p + 15);

	if (modem.modemType == MODEM_TYPE_KT) {
		RTC_incDateHour(&date, 9); // GMT+9
	}

	if (RTC_isValidDate(&date) == 0) {
		return 0;
	}

	if (RTC_needTimeSync()) {
		RTC_writeTime(date);
	}

	return 1;
}

static int parse_radioQuality(char *p)
{
#define PATTERN_RSRP "Signal power:"
#define PATTERN_RSRQ "RSRQ:"
#define PATTERN_SNR "SNR:"
#define PATTERN_RSSI "Total power:"
#define PATTERN_PCI "PCI:"
#define PATTERN_CGI "Cell ID:"
#define PATTERN_TX_POWER "TX power:"
	// NUESTATS:RADIO,Cell ID:51939434
	// NUESTATS:RADIO,TX power:-90

	char *cgi_pos = strstr(p, PATTERN_CGI);
	char *tx_power_pos = strstr(p, PATTERN_TX_POWER);
	char *rsrp_pos = strstr(p, PATTERN_RSRP);
	char *rsrq_pos = strstr(p, PATTERN_RSRQ);
	char *snr_pos = strstr(p, PATTERN_SNR);
	char *rssi_pos = strstr(p, PATTERN_RSSI);
	char *pci_pos = strstr(p, PATTERN_PCI);

	int result = 0;
	do {
		if (cgi_pos) {
			cgi_pos = cgi_pos + strlen(PATTERN_CGI);
			modem.modemQuality.cgi = atol(cgi_pos);
		} else {
			break;
		}

		if (pci_pos) {
			pci_pos = pci_pos + strlen(PATTERN_PCI);
			modem.modemQuality.cid = atoi(pci_pos);
		} else {
			break;
		}

		if (tx_power_pos) {
			tx_power_pos = tx_power_pos + strlen(PATTERN_TX_POWER);
			modem.modemQuality.txPower = atoi(tx_power_pos);
		} else {
			break;
		}

		if (rsrp_pos) {
			rsrp_pos = rsrp_pos + strlen(PATTERN_RSRP);
			modem.modemQuality.rsrp = atol(rsrp_pos);
		} else {
			break;
		}

		if (snr_pos) {
			snr_pos = snr_pos + strlen(PATTERN_SNR);
			modem.modemQuality.snr = atol(snr_pos);
		} else {
			break;
		}

		if (rsrq_pos) {
			rsrq_pos = rsrq_pos + strlen(PATTERN_RSRQ);
			modem.modemQuality.rsrq = atoi(rsrq_pos);
		} else {
			break;
		}

		if (rssi_pos) {
			rssi_pos = rssi_pos + strlen(PATTERN_RSSI);
			modem.modemQuality.lastRSSI = atoi(rssi_pos);
		} else {
			break;
		}

		result = 1;
	} while (0);

	return result;
}

void MODEM_response(char *pHead, int len)
{
	char *p = NULL;

	while (len) {
		if (*pHead != '\r' && *pHead != '\n' && *pHead != ' ') {
			break;
		}
		pHead++;
		len--;
	}

	static BOOL isAckOk = FALSE;
	static BOOL IsReboot = FALSE;

#define PATTERN_ACK_OK "OK"
	if (strstr(pHead, PATTERN_ACK_OK)) {
		isAckOk = TRUE;
	} else {
		isAckOk = FALSE;
	}

#define PATTERN_ERROR "ERROR"
	if (strstr(pHead, PATTERN_ERROR)) {
		modemCtx.status.error = 1;
	}

#define PATTERN_FOTA_RESET "REBOOT_CAUSE_SECURITY_FOTA_UPGRADE"
	if (strstr(pHead, PATTERN_FOTA_RESET)) {
		modemCtx.lwm2m.fotaFinish = 1;
	}

#define PATTERN_SERVER_NOTIFY "+QLWEVTIND:"
	p = pHead;
	do {
		if (p = strstr(p, PATTERN_SERVER_NOTIFY)) {
			parse_server_notify(p += strlen(PATTERN_SERVER_NOTIFY));
		}
	} while (p);

#define PATTERN_PF_DL "+NNMI:"
	if (p = strstr(pHead, PATTERN_PF_DL)) {
		parse_pf_downlink(p + strlen(PATTERN_PF_DL));
		modemCtx.pfDlCnt++;
	}

#define PATTERN_CHK_PSM_PREFIX "+NPSMR:"
#define PATTERN_CHK_PSM_PREFIX2 "+NPSMR:1,"
#define PATTERN_ENTER_PSM 1
	if (p = strstr(pHead, PATTERN_CHK_PSM_PREFIX2)) {
		p += strlen(PATTERN_CHK_PSM_PREFIX2);
		modemCtx.status.psmOn = (atoi(p) == PATTERN_ENTER_PSM) ? 1 : 0;
		printf_ts("MODEM : Get PSM Status(%d).\n", modemCtx.status.psmOn);
	} else if (p = strstr(pHead, PATTERN_CHK_PSM_PREFIX)) {
		p += strlen(PATTERN_CHK_PSM_PREFIX);
		modemCtx.status.psmOn = (atoi(p) == PATTERN_ENTER_PSM) ? 1 : 0;
		printf_ts("MODEM : event of changing PSM(%d).\n", modemCtx.status.psmOn);
	}

#define PATTERN_CEREG_NOT_REG_EVENT "+CEREG:0"
#define PATTERN_CEREG_ATTACH_EVENT "+CEREG:1"
#define PATTERN_CEREG_TRY_ATTACH_EVENT "+CEREG:2"
#define PATTERN_CEREG_REJECT_EVENT "+CEREG:3"
// No service event 발생 시 attach flag 초기화.
#define PATTERN_CEREG_NO_SERVICE_PREFIX "+CEREG:4"

	if (p = strstr(pHead, PATTERN_CEREG_ATTACH_EVENT)) {
		modemCtx.status.cellreg = MODEM_CELLREG_ATTACHED;
	} else if (p = strstr(pHead, PATTERN_CEREG_TRY_ATTACH_EVENT)) {
		modemCtx.lwm2m.regFinish = 0;
		modemCtx.lwm2m.obsObj10250 = 0;
		modemCtx.lwm2m.obsObj16241 = 0;
		modemCtx.status.cellreg = MODEM_CELLREG_PAUSE;
	} else if (p = strstr(pHead, PATTERN_CEREG_NOT_REG_EVENT)) {
		modemCtx.lwm2m.regFinish = 0;
		modemCtx.lwm2m.obsObj10250 = 0;
		modemCtx.lwm2m.obsObj16241 = 0;
		modemCtx.status.cellreg = MODEM_CELLREG_NOT_REG;
	} else if (p = strstr(pHead, PATTERN_CEREG_NO_SERVICE_PREFIX)) {
		modemCtx.lwm2m.regFinish = 0;
		modemCtx.lwm2m.obsObj10250 = 0;
		modemCtx.lwm2m.obsObj16241 = 0;
		modemCtx.status.cellreg = MODEM_CELLREG_NO_SERVICE;
	} else if (p = strstr(pHead, PATTERN_CEREG_REJECT_EVENT)) {
		modemCtx.lwm2m.regFinish = 0;
		modemCtx.lwm2m.obsObj10250 = 0;
		modemCtx.lwm2m.obsObj16241 = 0;
		modemCtx.status.cellreg = MODEM_CELLREG_REJECTED;
	}

	BOOL isRleaseBusy = FALSE;
	switch (modemComm.atCmd->index) {
	case AT_CMD_IDX_SW_RESET:
#define PATTERN_REBOOT_REPORT "REBOOT_CAUSE_APPLICATION_AT"
		if (strstr(pHead, PATTERN_REBOOT_REPORT)) {
			modemCtx.status.cellreg = 0;
			modemCtx.lwm2m.value &= 0x01; // bsFinish Flag no clear
			IsReboot = TRUE;
		}

		if (IsReboot && isAckOk) {
			IsReboot = FALSE;
			isRleaseBusy = TRUE;
		}
		break;

	case AT_CMD_IDX_GET_IMEI:
#define PATTERN_IMEI "+CGSN:"
		if (p = strstr(pHead, PATTERN_IMEI)) {
			if (parse_imei(p + strlen(PATTERN_IMEI))) {
				isRleaseBusy = TRUE;
			}
		}
		break;

	case AT_CMD_IDX_GET_IMSI:
#define PATTERN_IMSI IMSI_MCC_KOREA
		if (p = strstr(pHead, PATTERN_IMSI)) {
			if (parse_imsi(p)) {
				isRleaseBusy = TRUE;
			}
		}
		break;

	case AT_CMD_IDX_GET_ICCID:
#define PATTERN_ICCID "+NCCID:"
		if (p = strstr(pHead, PATTERN_ICCID)) {
			if (parse_iccid(p + strlen(PATTERN_ICCID))) {
				isRleaseBusy = TRUE;
			}
		}
		break;

	case AT_CMD_IDX_GET_RADIO_QUALITY:
#define PATTERN_NUESTATS_RADIO "NUESTATS:RADIO,"
		if (p = strstr(pHead, PATTERN_NUESTATS_RADIO)) {
			if (parse_radioQuality(p + strlen(PATTERN_NUESTATS_RADIO))) {
				isRleaseBusy = TRUE;
			}
		}
		break;

	case AT_CMD_IDX_GET_NW_ALARM:
#define PATTERN_CEREG_PREFIX "+CEREG:5"
		if ((p = strstr(pHead, PATTERN_CEREG_PREFIX))) {
			modemCtx.status.cellreg = parse_cereg(p, &modemCtx);
		}

		if (modemCtx.status.cellreg == MODEM_CELLREG_ATTACHED) {
			RTC_read(&modem.lastAttachTime);
			isRleaseBusy = TRUE;
		}
		break;

	case AT_CMD_IDX_GET_TIME:
#define PATTERN_CCLK "+CCLK:"
		if (p = strstr(pHead, PATTERN_CCLK)) {
			if (parse_cclk(p + strlen(PATTERN_CCLK))) {
				isRleaseBusy = TRUE;
			}
		}
		break;

	case AT_CMD_IDX_GET_CDP:
		if (modemCtx.status.error) {
			// In this case that first read when is not set
			isRleaseBusy = TRUE;
		} else {
#define PATTERN_READ_NCDP "+NCDP:"
			if (p = strstr(pHead, PATTERN_READ_NCDP)) {
				if (!parse_ncdp(p + strlen(PATTERN_READ_NCDP))) {
					modemCtx.proc.runInit = 1;
				}
			}

			if (isAckOk) {
				isRleaseBusy = TRUE;
			}
		}
		break;

	case AT_CMD_IDX_GET_LWM2M_SERVER:
		if (modemCtx.status.error) {
			// In this case that first read when is not set
			isRleaseBusy = TRUE;
		} else {
#define PATTERN_READ_BS_SERVER "+QLWSERVERIP:BS,"
			if (p = strstr(pHead, PATTERN_READ_BS_SERVER)) {
				if (!parse_lwm2m_server(p + strlen(PATTERN_READ_BS_SERVER))) {
					modemCtx.proc.runInit = 1;
				}
			}
			// LWM2M의 경우 플랫폼 접속 후 플랫폼 요청에 의해
			// 주소값이 변경될 수 있으므로 따로 확인하지 않는다.
			// #define PATTERN_READ_LWM2M_SERVER "+QLWSERVERIP:LWM2M,"

			if (isAckOk) {
				isRleaseBusy = TRUE;
			}
		}
		break;

	case AT_CMD_IDX_GET_EPN:
		if (modemCtx.status.error) {
			// In this case that first read when is not set
			isRleaseBusy = TRUE;
		} else {
#define PATTERN_READ_EPN "+QLWEPNS: "
			if (p = strstr(pHead, PATTERN_READ_EPN)) {
				p += strlen(PATTERN_READ_EPN);
				if (!parse_ep_name(p)) {
					modemCtx.proc.runInit = 1;
				}
			}

			if (isAckOk) {
				isRleaseBusy = TRUE;
			}
		}
		break;

	case AT_CMD_IDX_GET_BSPS:
		if (modemCtx.status.error) {
			// In this case that first read when is not set
			isRleaseBusy = TRUE;
		} else {
#define PATTERN_READ_BS_PARAM "+QLWMBSPS: "
			if (p = strstr(pHead, PATTERN_READ_BS_PARAM)) {
				p += strlen(PATTERN_READ_BS_PARAM);
				if (!parse_bs_param(p)) {
					modemCtx.proc.runInit = 1;
				}
			}

			if (isAckOk) {
				isRleaseBusy = TRUE;
			}
		}
		break;

	case AT_CMD_IDX_RUN_BOOTSTRAP:
		if (modemCtx.lwm2m.bsFinish) {
			isRleaseBusy = TRUE;
		}
		break;

	case AT_CMD_IDX_RUN_REGISTER:
#define BOOTUP_REGISTER_PARAM "=0"
#define DELETE_REGISTER_PARAM "=1"
#define UPDATE_REGISTER_PARAM "=2"

		if (strstr(modemComm.atData, BOOTUP_REGISTER_PARAM)) {
			if (modemCtx.lwm2m.regFinish && modemCtx.lwm2m.obsObj10250) {
				isRleaseBusy = TRUE;
			}
		} else if (strstr(modemComm.atData, DELETE_REGISTER_PARAM)) {
			if (modemCtx.lwm2m.regDelete) {
				isRleaseBusy = TRUE;
			}
		} else if (strstr(modemComm.atData, UPDATE_REGISTER_PARAM)) {
			if (modemCtx.lwm2m.regUpdate) {
				isRleaseBusy = TRUE;
			}
		} else { /* No Action */
		}
		break;

	case AT_CMD_IDX_SOCKET_SEND_UL:
		if (modemCtx.step == MODEM_STEP_UPDATE_QA) {
			if (isAckOk) {
				isRleaseBusy = TRUE;
			}
		} else {
			if (isAckOk) {
				modemCtx.ulCnt++;
			}

#define PATTERN_RECV_DL "+NSONMI:"
			if (p = strstr(pHead, PATTERN_RECV_DL)) {
				if (modemCtx.socket == atoi(p + strlen(PATTERN_RECV_DL))) {
					if (p = strstr(p, ",")) {
						modemComm.dlDataLen = atoi(++p);
						isRleaseBusy = TRUE;
						modemCtx.dlCnt++;
					}
				}
			}
		}
		break;

	case AT_CMD_IDX_SOCKET_RECV_DL:
		if (parse_udp_downlink(pHead)) {
			isRleaseBusy = TRUE;
		}
		break;

	case AT_CMD_IDX_SOCKET_CREATE:
#define PATTERN_CREATE_SOCKET "OK"
		if (p = strstr(pHead, PATTERN_CREATE_SOCKET)) {
			// 'OK'를 기준으로 '\r\n\r\n' 이전에 socket number 값이 존재.
			isRleaseBusy = TRUE;
			modemCtx.socket = atoi(p -= 5);
		}
		break;

	case AT_CMD_IDX_CHECK_BIPST:
#define PATTERN_BIPST_FINISH "+MBIPST:0"
		if (strstr(pHead, PATTERN_BIPST_FINISH)) {
			isRleaseBusy = TRUE;
		}
		break;

	case AT_CMD_IDX_GET_SWITCH_LWM2M:
#define PATTERN_SWT_LWM2M "+QREGSWT:"
#define FLAG_DISABLE_LWM2M 2
		if (p = strstr(pHead, PATTERN_SWT_LWM2M)) {
			isRleaseBusy = TRUE;
			int status = atoi(p + strlen(PATTERN_SWT_LWM2M));
			if (status != FLAG_DISABLE_LWM2M) {
				modemCtx.status.lwm2mOn = 1;
				if (!modemCtx.proc.runFota) {
					modemCtx.proc.runInit = 1;
				}
			} else {
				modemCtx.status.lwm2mOn = 0;
				if (modemCtx.proc.runFota) {
					modemCtx.proc.runInit = 1;
				}
			}
		}
		break;

	case AT_CMD_IDX_RUN_DATA_NOTI:
		if (isAckOk) {
			isRleaseBusy = TRUE;
			modemCtx.pfUlCnt++;
		}
		break;

	case AT_CMD_IDX_QLWULDATAEX:
		if (parseQLWULDATAEX(pHead, &modemCtx)) {
			isRleaseBusy = TRUE;
		}
		break;

	case AT_CMD_IDX_DETACH_NW:
		if (isAckOk) {
			isRleaseBusy = TRUE;
			/* modemCtx.status.cellreg = 0; */
		}
		break;

	case AT_CMD_IDX_GET_REPORT_PSM:
		if (p = strstr(pHead, PATTERN_CHK_PSM_PREFIX)) {
			if (modemCtx.status.psmOn == 0) {
				isRleaseBusy = TRUE;
			}
		}
		break;

	case AT_CMD_IDX_SET_PSM:
	case AT_CMD_IDX_SET_BAND:
	case AT_CMD_IDX_SET_RESELECT:
	case AT_CMD_IDX_SET_CDP:
	case AT_CMD_IDX_SET_EPN:
	case AT_CMD_IDX_SET_BSPS:
	case AT_CMD_IDX_SET_LWM2M:
	case AT_CMD_IDX_CHECK_ALIVE:
	case AT_CMD_IDX_SOCKET_CLOSE:
	case AT_CMD_IDX_SET_RF_CTRL:
	case AT_CMD_IDX_SET_NW_ALARM:
	case AT_CMD_IDX_GET_FW_REV:
	case AT_CMD_IDX_SET_REPORT_PSM:
	case AT_CMD_IDX_SET_LWM2M_SERVER:
	case AT_CMD_IDX_SWITCH_LWM2M:
	case AT_CMD_IDX_GET_NM_STATUS:
	case AT_CMD_IDX_GET_DATA_STATUS:
	case AT_CMD_IDX_SET_FOTA:
		if (isAckOk) {
			isRleaseBusy = TRUE;
		}
		break;

	default:
		break;
	}

	if (isRleaseBusy) {
		modemCtx.status.busy = 0;
	}
}

#endif

// TDD_TEST

unsigned char parse_cereg(char *p, ModemContext_t *modemPtr)
{
#define CEREG_STATUS_POS 1
#define CEREG_REJECT_CAUSE_POS 6
#define ATTACH_CEREG_PREFIX "CEREG:5"

	int pos = 0;
	char *token = strtok(p, ",");
	unsigned char status = MODEM_CELLREG_NOT_REG;

	while (token != NULL) {
		int state = atoi(token);
		tddPrint("token : %-10s pos : %-2d state : %-4d len : %-2ld\n", token, pos, state,
			 strlen(token));

		if ((pos == CEREG_STATUS_POS) && (strlen(token) == 1)) {
			if (state == MODEM_CELLREG_ATTACHED) {
				status = MODEM_CELLREG_ATTACHED;
			} else if (pos == CEREG_STATUS_POS) {
				status = state;
			}
		}

		if (++pos > CEREG_STATUS_POS) {
			break;
		}
		token = strtok(NULL, ",");
	}

	tddPrint("attach status : %d\n", status);

	if (status != MODEM_CELLREG_ATTACHED) {
		modemPtr->lwm2m.regFinish = 0;
		modemPtr->lwm2m.obsObj10250 = 0;
		modemPtr->lwm2m.obsObj16241 = 0;

		tddPrint("flag clear regFinish : %d obj10250 : %d obj16241 : %d\n",
			 modemPtr->lwm2m.regFinish, modemPtr->lwm2m.obsObj10250,
			 modemPtr->lwm2m.obsObj16241);
	}

	return status;
}

BOOL parseQLWULDATAEX(const char *pHead, ModemContext_t *modemPtr)
{
#define HAVE_NOT_BEEN_SENT 0
#define WAIT_RESPONSE_PLATFORM 1
#define SENT_FAILED 2
#define TIMEOUT 3
#define SEND_SUCCESS 4
#define GOT_RESET_MSG 5

#define LWM2M_UPLINK_STATUS "+QLWULDATASTATUS:"

	tddPrint("recv : %s\n", pHead);
	char *p = NULL;
	BOOL retValue = FALSE;

	if ((p = strstr(pHead, LWM2M_UPLINK_STATUS)) != NULL) {
		retValue = TRUE;

		p += strlen(LWM2M_UPLINK_STATUS);
		int status = atoi(p);
		tddPrint("status : %d\n", status);

		if (status == SEND_SUCCESS) {
			tddPrint("uplink ok, wait downlink\n");
			modemPtr->waitDl = TRUE;
		} else {
			tddPrint("uplink fail\n");
		}
	}

	return retValue;
}

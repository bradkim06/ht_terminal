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
#include "battery.h"
#include "test.h"
#include "message.h"
#include "loraModem.h"

extern Config_t conf;
extern uint8 AppProcess;

BOOL MODEM_parseDateTime(char *p)
{
#define GPS_BASE_SEC (315932400L) // 1980-01-06 00:00:00
#define GMT_9 (9 * 3600L)
#define LEAP_SEC 17

	uint32 rawSec = 0;
	while (1) {
		if (*p < '0' || *p > '9') {
			break;
		}
		rawSec *= 10;
		rawSec += (*p - '0');
		p++;
	}

	// GMT_9를 한 번만 더하는 게 맞을 것 같은데 두 번을 더해야 제대로 된 시간이 변환됨
	// IAR의 localtime에서 GMT 관련 처리가 있는 듯함

	rawSec = rawSec + GPS_BASE_SEC + GMT_9 + GMT_9 - LEAP_SEC;

	time_t t_sec = (time_t)rawSec;
	struct tm *t_date = localtime(&t_sec);

	Date_t date;
	memset(&date, 0, sizeof(Date_t));

	date.year = t_date->tm_year + 1900;
	date.mon = t_date->tm_mon + 1;
	date.day = t_date->tm_mday;
	date.hour = t_date->tm_hour;
	date.min = t_date->tm_min;
	date.sec = t_date->tm_sec;

	if (conf.reportSec <= 45) {
		// 같은 '분'에 보고하는 단말기들간 15초 단위 시간 분배를 위해 RTC를 변경
		RTC_incDateTime(&date, conf.reportSec);
	}

	printf_ts("DEVTIME(%04d-%02d-%02d %02d:%02d:%02d) - second offset:%d\n", date.year,
		  date.mon, date.day, date.hour, date.min, date.sec, conf.reportSec);

	RTC_writeTime(date);
	return TRUE;
}

// only use this section
typedef struct {
	uint8 rfu : 6, ver : 2;
	uint8 msgType;
	uint8 len;
	uint8 payload[0x100];
} LoraDevMgmtMsg_t;

// application message for device management
#define DEV_RESET 0x80
#define REP_PER_CHANGE 0x81
#define REP_IMMEDIATE 0x82

void MODEM_parseDeviceManagementMessage(uint8 *msg, int len)
{
	LoraDevMgmtMsg_t *p = (LoraDevMgmtMsg_t *)msg;
	switch (p->msgType) {
	case DEV_RESET:
		printf_ts("reset command received\n");
		// solu-m 모뎀이 uplink로 ack를 전송할 시간을 주기 위해 기다림.
		MISC_delayMs(6 * 1000);
		REBOOT_SYSTEM(); //20161005 Cho
		break;

	case REP_PER_CHANGE:
		printf_ts("change report period\n");
#if 0
            FLASH_readConfigInfo(&conf);
            config.txInterval = arg;
            FLASH_saveConfigInfo(&conf);
#endif
		break;

	case REP_IMMEDIATE:
		printf_ts("send report immediately\n");
		if (AppProcess == APP_IDLE) {
			MODEM_open(APP_IMMEDIATE_REPORT);
		}
		break;

	default:
		printf_ts("unknown device management message(0x%02X)\n", p->msgType);
		break;
	}
}

int parsePortnumber(char *p)
{
	if (strncmp(p, "0x", 2) == 0) {
		p += 2;
	} else if (strncmp(p, "0X", 2) == 0) {
		p += 2;
	} else if (strncmp(p + 1, "0x", 2) == 0) {
		p += 3;
	} else if (strncmp(p + 1, "0X", 2) == 0) {
		p += 3;
	}

	return ascii2BCD(*p, *(p + 1));
}

BOOL parse_sig(char *p)
{
	int8 snr = atoi(p);

	p = strstr(p, "RSSI : ");
	if (p == NULL) {
		return FALSE;
	}

	p += strlen("RSSI : ");
	if (*p == '-') {
		p++;
	}

	uint8 rssi = atoi(p);

	// RSSI, SNR 값이 모두 정상인 경우에만 저장 및 Flag set
	modem.sigQuality.snr = snr;
	modem.sigQuality.rssi = rssi;
	return TRUE;
}

void parse_deviceManagementCommand(char *pHead)
{
	int rxLen;
	uchar rxData[0x40] = "";

	do {
		char *p = strstr(pHead, "PAYLOAD_LEN : ");
		if (p == NULL) {
			break;
		}
		p += strlen("PAYLOAD_LEN : ");
		rxLen = atoi(p);
		if (rxLen == 0 || rxLen > 20) {
			rxLen = 0;
			break;
		}

		p = strstr(pHead, "SKT_EXT_DEVMGMT");
		if (p == NULL) {
			break;
		}

		p += strlen("SKT_EXT_DEVMGMT");
		while (*p == '\r' || *p == '\n' || *p == ' ') {
			p++;
		}

		for (int i = 0; i < rxLen; i++) {
			rxData[i] = ascii2BCD(*(p + i * 2 + 0), *(p + i * 2 + 1));
		}
	} while (0);

	if (rxLen) {
#if 1
		printf_ts("RX_COMMAND:");
		for (int i = 0; i < rxLen; i++) {
			printf(" %02X", rxData[i]);
		}
		printf("\n");
#endif

		MODEM_rcvDeviceControl(rxData, rxLen);
	}
}

void extract_digit(char *p, char *str, int len)
{
	int nDigit = 0;
	while (nDigit < len) {
		if (isxdigit(*p)) {
			*(str + nDigit++) = *p;
		}
		p++;
	}
}

BOOL parse_appKey(char *p)
{
	char str[32];
	extract_digit(p, str, 32);

	printf_ts("MODEM : AppKey=");
	for (int i = 0; i < 16; i++) {
		modem.appKey[i] = ascii2BCD(str[2 * i + 0], str[2 * i + 1]);
		printf("%02X", modem.appKey[i]);
	}
	printf("\n");

	return TRUE;
}

BOOL parse_appEui(char *p)
{
	char str[16];
	extract_digit(p, str, 16);

	printf_ts("MODEM : AppEUI=");
	for (int i = 0; i < 8; i++) {
		modem.appEui[i] = ascii2BCD(str[2 * i + 0], str[2 * i + 1]);
		printf("%02X", modem.appEui[i]);
	}
	printf("\n");

	return TRUE;
}

BOOL parse_devEui(char *p)
{
	char str[16];
	extract_digit(p, str, 16);

	printf_ts("MODEM : DevEUI=");
	for (int i = 0; i < 8; i++) {
		modem.devEui[i] = ascii2BCD(str[2 * i + 0], str[2 * i + 1]);
		printf("%02X", modem.devEui[i]);
	}
	printf("\n");

	if (memcmp(conf.devEui, modem.devEui, 8) != 0) {
		memcpy(conf.devEui, modem.devEui, 8);
		FLASH_saveConfigInfo(&conf);

		printf_ts("MODEM : Dev EUI changed\n");
	}

	return TRUE;
}

void MODEM_response(char *pHead, int len)
{
	char *p = NULL;

	// VERSION(debug=0), LOG Message(debug=1)
	if (modemComm.lastAtCmd->index == ATCMD_IDX_CHECK_ALIVE) {
		if (strstr(pHead, "Firmware Information") || strstr(pHead, "LOG Message Enable")) {
			modemCtx.status.alive = 1;
			modemCtx.status.busy = 0;
		}
	}

	if (strstr(pHead, "JOINED")) {
		modemCtx.status.provision = 1;
	}

	// 전원 on 직후에는 반드시 JOINED로만 확인하게 함.
	if (strstr(pHead, "Real Join Done")) {
		modemCtx.status.provision = 1;
	}

	if (modemComm.lastAtCmd->index == ATCMD_IDX_GET_SIG) {
		if ((p = strstr(pHead, "SNR : "))) {
			if (parse_sig(p + strlen("SNR : "))) {
				modemCtx.status.getSig = 1;
				modemCtx.status.busy = 0;
			}
		}
	}

	if (modemComm.lastAtCmd->index == ATCMD_IDX_SET_DR) {
		if ((p = strstr(pHead, "SET TX DATA RATE"))) {
			modemCtx.config.setDr = 1;
			modemCtx.status.busy = 0;
		}
	}

	if (modemComm.lastAtCmd->index == ATCMD_IDX_SET_ADR) {
		if ((p = strstr(pHead, "SET ADAPTIVE DATA RATE FLAG"))) {
			modemCtx.config.setAdr = 1;
			modemCtx.status.busy = 0;
		}
	}

	if (modemComm.lastAtCmd->index == ATCMD_IDX_SET_APPEUI) {
		if ((p = strstr(pHead, "SET APPLICATION EUI"))) {
			modemCtx.config.setAppEui = 1;
			modemCtx.status.busy = 0;
		}
	}

	if (modemComm.lastAtCmd->index == ATCMD_IDX_SET_APPKEY) {
		if ((p = strstr(pHead, "SET APPLICATION KEY"))) {
			modemCtx.config.setAppKey = 1;
			modemCtx.status.busy = 0;
		}
	}

	if (modemComm.lastAtCmd->index == ATCMD_IDX_SAVE_CFG) {
		if ((p = strstr(pHead, "Save configuration"))) {
			modemCtx.config.saveCfg = 1;
			modemCtx.status.busy = 0;
		}
	}

	if (modemComm.lastAtCmd->index == ATCMD_IDX_RESET_NWK) {
		if ((p = strstr(pHead, "SET Provisioning Status"))) {
			modemCtx.config.resetNwk = 1;
			modemCtx.status.busy = 0;
		}
	}

	if (modemComm.lastAtCmd->index == ATCMD_IDX_SW_RESET) {
		if ((p = strstr(pHead, "RESET OK"))) {
			modemCtx.status.busy = 0;
		}
	}

	// Rx abort 또한 timeout으로 처리.
	if (strstr(pHead, "OnRadioRxTimeout") || strstr(pHead, "PrepareRxDoneAbort")) {
		modemCtx.status.timeout = 1;
	}

	if (modemComm.lastAtCmd->index == ATCMD_IDX_SEND_DATA) {
		if (strstr(pHead, "RX DONE")) {
			modemCtx.status.acked = 1;
			modemCtx.status.busy = 0;
		}
	}

	if (modemComm.lastAtCmd->index == ATCMD_IDX_GET_TIME) {
		if (p = strstr(pHead, "DEVTIME:")) {
			if (MODEM_parseDateTime(p + strlen("DEVTIME:"))) {
				modemCtx.status.timeSync = 1;
				modemCtx.status.busy = 0;
			}
		}
	}

	if (modemComm.lastAtCmd->index == ATCMD_IDX_GET_DEVEUI) {
		if (p = strstr(pHead, "Device EUI : ")) {
			if (parse_devEui(p + strlen("Device EUI : "))) {
				modemCtx.config.getDevEui = 1;
				modemCtx.status.busy = 0;
			}
		}
	}

	if (modemComm.lastAtCmd->index == ATCMD_IDX_GET_APPEUI) {
		if (p = strstr(pHead, "APPLICATION EUI : ")) {
			if (parse_appEui(p + strlen("APPLICATION EUI : "))) {
				modemCtx.config.getAppEui = 1;
				modemCtx.status.busy = 0;
			}
		}
	}

	if (modemComm.lastAtCmd->index == ATCMD_IDX_GET_APPKEY) {
		if (p = strstr(pHead, "Application Key : ")) {
			if (parse_appKey(p + strlen("Application Key : "))) {
				modemCtx.config.getAppKey = 1;
				modemCtx.status.busy = 0;
			}
		}
	}

	if (p = strstr(pHead, "SKT_DEV_RESET")) {
		printf_ts("Reset command received\n");
		REBOOT_SYSTEM();
	}

	if (p = strstr(pHead, "SKT_EXT_DEVMGMT")) {
		parse_deviceManagementCommand(pHead);
	}
}

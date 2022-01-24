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

extern Config_t conf;
extern uint8 AppProcess;

extern Modem_t modem;
extern ModemFlag_t mFlag;

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

	if (conf.secOffset <= 45) {
		// 같은 '분'에 보고하는 단말기들간 15초 단위 시간 분배를 위해 RTC를 변경
		RTC_incDateTime(&date, conf.secOffset);
	}

	printf("dateTime [%04d-%02d-%02d %02d:%02d:%02d] - secOffset:%d\n", date.year, date.mon,
	       date.day, date.hour, date.min, date.sec, conf.secOffset);

	RTC_writeTime(date);
	return TRUE;
}

void MODEM_parseDeviceManagementMessage(uint8 *msg, int len)
{
	LoraDevMgmtMsg_t *p = (LoraDevMgmtMsg_t *)msg;
	switch (p->msgType) {
	case DEV_RESET:
		printf("reset command received\n");
		// solu-m 모뎀이 uplink로 ack를 전송할 시간을 주기 위해 기다림.
		MISC_delayMs(6 * 1000);
		REBOOT_SYSTEM(); //20161005 Cho
		break;

	case REP_PER_CHANGE:
		printf("change report period\n");
#if 0
            FLASH_readConfigInfo(&conf);
            config.txInterval = arg;
            FLASH_saveConfigInfo(&conf);
#endif
		break;

	case REP_IMMEDIATE:
		printf("send report immediately\n");
		if (AppProcess == APP_IDLE) {
			MODEM_open(APP_IMMEDIATE_REPORT);
		}
		break;

	default: {
		printf("unknown device management message(0x%02X)\n", p->msgType);
	}
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

void parse_SNR_RSSI(char *pHead)
{
	char *p = strstr(pHead, "SNR:");
	if (p) {
		p += strlen("SNR:");
		int8 snr = atoi(p);
		if (snr != 0) {
			modem.lastSNR = snr;
			mFlag.gotSnr = 1;
		}
	}

	p = strstr(pHead, "RSSI:");
	if (p) {
		p += strlen("RSSI:");
		if (*p == '-') {
			p++;
		}

		uint8 rssi = atoi(p);
		if (rssi != 0) {
			modem.lastRSSI = rssi;
			mFlag.gotRssi = 1;
		}
	}

#if 0
    if(mFlag.gotSnr && mFlag.gotRssi) {
        printf("SNR: %d, RSSI:%d\n", modem.lastSNR, modem.lastRSSI);
    }
#endif
}

/**
 * @brief Check and parsing downlink payload
 * @code
 *   char* pData = NULL;
 *   int16 len = parse_Downlink((const char *) pHead, &pData);
 *   if (len > 0) {
 *      free(pData);
 *   }
 * @endcode
 * @param pHead  String data head pointer.
 * @return Downlink payload array head pointer.
 * @warning Have to free "pData"
 */
char *parse_Downlink(const char *pHead)
{
	// Downlink log format example
	//   RX: Fport:1,size:10
	//   31323036323238333831
	// Payload is next line and payload format is Hex string.

	char *p = strstr(pHead, "size:");
	if (p) {
		// Get Downlink payload length
		p += strlen("size:");
		int16 len = atoi(p);
		if (len > 0) {
			// Memory alloc ('+1' is for data end)
			char *pData = (char *)malloc(sizeof(char) * len + 1);

			// Move to downlink payload
			while (*(p++) != '\n')
				;

			// Convert hex string payload to hex array
			// the payload is hex data but, received string from MODEM
			// So, in this code, convert hex stirng to hex data array.
			int idx = 0;
			while (TRUE) {
				*(pData + idx) = (*(p++) - '0') * 16;
				*(pData + idx) += (*(p++) - '0');
				if (idx++ >= len)
					break;
			}

			printf_ts("DL : len = %d, payload = %s \n", len, pData);

			return pData;
		}
	}

	return NULL;
}

void SOLUM_MODEM_parse(char *pHead, int len)
{
	char *p = NULL;

	if (strstr(pHead, "OK")) {
		mFlag.alive = 1;
	}

	if (strstr(pHead, "JOINED")) {
		modem.joined = 1;
#if 0
        // AS923의 Join은 반드시 DR2(SF10)에서 수행됨. 따라서 Join 완료 후 Data Rate를 가장
        // 낮은 값으로 설정할 필요가 있을지 추후 결정 필요.

        MODEM_write("AT+DR 0");    // ADR enable
        MISC_delayMs(500);
#endif
	}

	if ((p = strstr(pHead, "SNR:"))) {
		parse_SNR_RSSI(pHead);
	}

	if (strstr(pHead, "OnRadioRxTimeout")) {
		mFlag.timeoutOccurred = 1;
	}

	if (strstr(pHead, "SEND")) {
		if (strstr(pHead, "SUCCESS")) {
			mFlag.acked = 1;
			// TODO : Clear stored meter data when predic report
			METER_clearStoredData();
		} else if (strstr(pHead, "ERROR") || strstr(pHead, "FAIL")) {
			// SEND?ERROR : 전송 중에 또 다른 송신 요구를 함. (RF Busy)
			// SEND?FAIL - 8회 재전송 후 전송 실패를 나타냄.
			printf_ts("TX Failed %s\n", strstr(pHead, "ERROR") ? "(RF Busy)" : "");

			OSAL_stopEventTimer(AppTaskId, APP_EVENT_MODEM_TIMEOUT);
			OSAL_setEvent(AppTaskId, APP_EVENT_MODEM_TIMEOUT);
		}
	}

	// Downlink payload 관련 log가 출력될 경우 현재 step과 관계 없이 parsing한다.
	// Parsing 후 동작은 Modem step이나 기타 조건에 따라 동작한다.
	if (strstr(pHead, "RX:")) {
		char *pData = parse_Downlink(pHead);
		if (pData != NULL) {
			if (modem.step == MODEM_STEP_GET_TIME) {
				if (MODEM_parseDateTime(pData)) {
					mFlag.gotTime = 1;
				}
			}

			// Have to free pData
			free(pData);
		}
	}

	if (p = strstr(pHead, "+DEUI=?")) {
		parse_devEui(p + strlen("+DEUI=?"));
		mFlag.gotDevEui = 1;
	}

	if (p = strstr(pHead, "+AEUI=?")) {
		parse_appEui(p + strlen("+AEUI=?"));
		mFlag.gotAppEui = 1;
	}
}

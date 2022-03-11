#include "RTC.h"
#include "common_header.h"
#include "MSP430FlashUtil.h"

#include "port_desc.h"
#include "check_meter_misc.h"
#include "uart.h"
#include "lcdDriver.h"

#include "osal_Timer.h"
#include "Task_Mgr.h"

#include "app.h"
#include "shell.h"
#include "test.h"
#include "mTest.h"
#include "menu.h"
#include "dataFlash.h"

#if defined(AUX_REPEATER)
#include "rfTest.h"
#endif

#define PREV_KEY 0x5B /* "[" key */
#define NEXT_KEY 0x5D /* "]" key */

#define CLEAR_LINE() printf("\33[2K\r");
#define PROMPT() printf("\r[Hitec]%% ");

static ShellCmdBuffer_t *ShellCmdBuf = NULL;

void SHELL_init()
{
	ShellCmdBuf = (ShellCmdBuffer_t *)malloc(sizeof(ShellCmdBuffer_t));
	if (ShellCmdBuf == NULL) {
		printf("malloc(ShellCmdBuf) failed\n");
		REBOOT_SYSTEM();
	}
	memset(ShellCmdBuf, 0, sizeof(ShellCmdBuffer_t));
}

char SHELL_getChar()
{
	if (TEST_checkTestMode() == FALSE) {
		REBOOT_SYSTEM();
	}

	TEST_runTasks();

	char c;
	if (!UART_getChar(UART_A1, &c)) {
		return 0;
	}

	static int escMode = 0;

	switch (escMode) {
	case 0:
		if (c == 0x1B) {
			escMode++;
			c = 0;
		}
		break;

	case 1:
		if (c == 0x5B) {
			escMode++;
		} else {
			escMode = 0;
		}
		c = 0;
		break;

	default:
		switch (c) {
		case 'A':
			c = CHAR_UP_ARROW;
			break;
		case 'B':
			c = CHAR_DOWN_ARROW;
			break;
		case 'C':
			c = CHAR_RIGHT_ARROW;
			break;
		case 'D':
			c = CHAR_LEFT_ARROW;
			break;
		default:
			c = 0;
		}
		escMode = 0;
	}
	return c;
}

int SHELL_gets(char *buf)
{
	char c;
	char *s = buf;
	int hptr = (ShellCmdBuf->wptr > 0) ? ((ShellCmdBuf->wptr - 1) % MAX_HISTORY) : 0;
	ShellCmdBuffer_t *h = ShellCmdBuf;

	while (1) {
		c = SHELL_getChar();
		if (c == 0) {
			continue;
		}

		if (c == '\n' || c == '\r') {
			printf("\n");
			break;
		}

		switch (c) {
		case CHAR_UP_ARROW:
		case CHAR_LEFT_ARROW:
			CLEAR_LINE();
			PROMPT();
			snprintf(buf, MAX_COLUMN, "%s", h->cmd[hptr]);
			if (--hptr < 0) {
				hptr = MAX_HISTORY - 1;
			}
			printf("%s", buf);
			s = buf + strlen(buf);
			break;

		case CHAR_DOWN_ARROW:
		case CHAR_RIGHT_ARROW:
			CLEAR_LINE();
			PROMPT();
			snprintf(buf, MAX_COLUMN, "%s", h->cmd[hptr]);
			if (++hptr >= MAX_HISTORY) {
				hptr = 0;
			}
			printf("%s", buf);
			s = buf + strlen(buf);
			break;

		case PREV_KEY:
			CLEAR_LINE();
			PROMPT();
			snprintf(buf, MAX_COLUMN, "%s", h->cmd[hptr]);
			if (--hptr < 0) {
				hptr = MAX_HISTORY - 1;
			}
			printf("%s", buf);
			s = buf + strlen(buf);
			break;

		case NEXT_KEY:
			CLEAR_LINE();
			PROMPT();
			snprintf(buf, MAX_COLUMN, "%s", h->cmd[hptr]);
			if (++hptr >= MAX_HISTORY) {
				hptr = 0;
			}
			printf("%s", buf);
			s = buf + strlen(buf);
			break;

		case '\r':
			printf("%c", c);
			break;

		case '\b':
			if (s > buf) {
				printf("%c", c);
				--s;
				printf(" \b");
			}
			break;

		case 0x7f:
			while (s > buf) {
				--s;
				printf("\b \b");
			}
			break;

		default:
			if (c >= ' ' && c <= '~')
				*s = c;
			if (s < buf + MAX_COLUMN - 1)
				s++;
			else {
				printf("\b");
			}
			printf("%c", c);
		}
	}

	h->hptr = h->wptr;
	*s = '\0';
	return ((int)(s - buf));
}

uint32 SHELL_atoi(char *p)
{
	uint32 c, n;
	uint32 b;

	b = 16;
	n = 0;

	switch (*p) {
	case '&':
		p += 1;
		b = 10;
		break;
	case '$':
		p += 1;
		b = 16;
		break;
	}

	for (; c = *p; p++) {
		if (c >= '0' && c <= '9')
			c -= '0';
		else if (c >= 'A' && c <= 'F')
			c -= ('A' - 10);
		else if (c >= 'a' && c <= 'f')
			c -= ('a' - 10);
		else
			break;
		if (c >= b)
			break;
		n = n * b + c;
	}

	return (n);
}

void SHELL_string(char *p, char *strbuf)
{
	*strbuf = '\0';

	while (*p && *p != 0x20) {
		*strbuf++ = *p++;
	}
	*strbuf = '\0';
}

uint32 SHELL_decimal(char *p)
{
	uint32 c, n;

	n = 0;

	for (; c = *p; p++) {
		if (c >= '0' && c <= '9')
			c -= '0';
		else if (c == ' ')
			break;
		else if (c == '.')
			continue;
		else
			return 0; //(uint)-1;
		n = n * 10 + c;
	}

	return (n);
}

static int getArguments(char *p, char **cmdArgs, int maxArgs)
{
	int numOfArguments = 0;
	while (1) {
		// search space
		while (*p != 0x20) {
			if (*p == '\0' || *p == '\r' || *p == '\n')
				return numOfArguments;
			p++;
		}

		while (1) {
			if (*p == '\0' || *p == '\r' || *p == '\n')
				return numOfArguments;
			if (*p != 0x20) {
				cmdArgs[numOfArguments++] = p;
				break;
			}
			p++;
		}

		if (numOfArguments >= maxArgs)
			return numOfArguments;
	}
}

static void help()
{
	printf("----------------------------------------------------------------------------\n");
	printf("                                Command set                                 \n");
	printf("----------------------------------------------------------------------------\n");

	int nCmd = sizeof(CmdSet) / sizeof(ShellCommandSet_t);
	for (int i = 0; i < nCmd; i++) {
		if (strlen(CmdSet[i].usage)) {
			printf("%s\n", CmdSet[i].usage);
		}
	}
	printf("----------------------------------------------------------------------------\n");
}

static int whatCommand(char *p)
{
	int i;
	char tmp[0x10];

	i = 0;
	while (*p != 0x20 && *p != '\0' && i < 0x10)
		tmp[i++] = *p++;
	tmp[i] = '\0';

	int nCmd = sizeof(CmdSet) / sizeof(ShellCommandSet_t);
	for (i = 0; i < nCmd; i++) {
		if (!strcmp(CmdSet[i].cmd, tmp)) {
			return i;
		}
	}
	return -1;
}

void SHELL_run()
{
#define LEN_ARG_STRING 0x30
#define MAX_NUM_OF_ARGS 10

	int cmdIdx;
	int numOfCmdArgs;
	char *cmdArgs[MAX_NUM_OF_ARGS];
	char cmdBuf[MAX_COLUMN];
	ShellCmdBuffer_t *h = ShellCmdBuf;

	while (1) {
		PROMPT();

		if (SHELL_gets(cmdBuf) == 0) {
			continue;
		}

		snprintf(h->cmd[h->wptr], MAX_COLUMN, "%s", cmdBuf);
		if (++h->wptr >= MAX_HISTORY) {
			h->wptr = 0;
		}

		cmdIdx = whatCommand(cmdBuf);
		numOfCmdArgs = getArguments(cmdBuf, cmdArgs, MAX_NUM_OF_ARGS);
		switch (cmdIdx) {
		case CMDN_QMARK:
		case CMDN_HELP:
			help();
			break;

		case CMDN_RTC:
		case CMDN_PWRTC:
			if (numOfCmdArgs >= 6) {
				int year = SHELL_decimal(cmdArgs[0]); /* year */
				int month = SHELL_decimal(cmdArgs[1]); /* month */
				int day = SHELL_decimal(cmdArgs[2]); /* day */
				int hour = SHELL_decimal(cmdArgs[3]); /* hour */
				int min = SHELL_decimal(cmdArgs[4]); /* minute */
				int sec = SHELL_decimal(cmdArgs[5]); /* second */
				TEST_setRTC((cmdIdx == CMDN_PWRTC) ? 1 : 0, year, month, day, hour,
					    min, sec);
			} else {
				if (numOfCmdArgs == 0) {
					TEST_readRTC(0);
				} else {
					goto SHELL_CMD_ERROR;
				}
			}
			break;

		case CMDN_RFLASH:
			if (numOfCmdArgs >= 1) {
				int sector = SHELL_decimal(cmdArgs[0]); /* sector */
				TEST_readFlash(sector);
			} else {
				TEST_readFlash(0xFF);
			}
			break;

		case CMDN_EFLASH:
			if (numOfCmdArgs >= 1) {
				int sector = SHELL_decimal(cmdArgs[0]); /* sector */
				TEST_eraseFlash(sector);
			} else {
				TEST_eraseFlash(0xFF);
			}
			break;

		case CMDN_BATT:
		case CMDN_PBATT:
			TEST_readBattery();
			break;

		case CMDN_LPM3:
		case CMDN_PLPM3:
			if (numOfCmdArgs >= 1) {
				int mode = SHELL_decimal(cmdArgs[0]); /* sector */
				TEST_lpm3(mode);
			}
			break;

		case CMDN_PLPM2:
			TEST_lpm2();
			break;

		case CMDN_REED:
		case CMDN_PREED:
			TEST_reedSensor();
			break;

		case CMDN_RSN:
			TEST_readSerialNum();
			break;

		case CMDN_WSN:
			if (numOfCmdArgs >= 1) {
				char sn[LEN_ARG_STRING];
				SHELL_string(cmdArgs[0], sn); /* serial id */
				TEST_writeSerialNum(sn);
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;

		case CMDN_RESET:
		case CMDN_PRESET:
			REBOOT_SYSTEM();
			break;

		case CMDN_LCD:
		case CMDN_PLCD:
			TEST_lcd();
			break;

		case CMDN_PMLCD:
			TEST_meterLCD();
			break;

		case CMDN_CONFIG:
			TEST_config();
			break;

		case CMDN_MP:
			if (numOfCmdArgs >= 2) {
				int ri = SHELL_decimal(cmdArgs[0]); /* ri */
				int mi = SHELL_decimal(cmdArgs[1]); /* mi */
				TEST_meteringParamer(ri, mi);
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;

		case CMDN_SLEEP:
			if (numOfCmdArgs >= 1) {
				int flag = SHELL_decimal(cmdArgs[0]);
				TEST_sleepMode(flag);
			} else {
				TEST_sleepMode(0xFF);
			}
			break;

		case CMDN_MT:
			if (numOfCmdArgs >= 1) {
				int type = SHELL_decimal(cmdArgs[0]); /* meter type */
				TEST_setMeterType(type);
			} else {
				TEST_setMeterType(0xFF);
			}
			break;

		case CMDN_SI:
			if (numOfCmdArgs >= 1) {
				int flag = SHELL_decimal(cmdArgs[0]);
				TEST_shortInterval(flag);
			} else {
				TEST_shortInterval(0xFF);
			}
			break;

		case CMDN_DEBUG:
			if (numOfCmdArgs >= 1) {
				int flag = SHELL_decimal(cmdArgs[0]);
				TEST_debugPrint(flag);
			} else {
				TEST_debugPrint(0xFF);
			}
			break;

		case CMDN_READ_METER:
			if (numOfCmdArgs >= 1) {
				int type = SHELL_decimal(cmdArgs[0]); /* meter type */
				TEST_readMeter(type);
			} else {
				TEST_readMeter(0xFF);
			}
			break;

		case CMDN_MODEM:
			TEST_modem();
			break;

		case CMDN_PVER:
			TEST_readFwVersion();
			break;

		case CMDN_PWMETER:
		case CMDN_PMETER:
			if (numOfCmdArgs >= 3) {
				// 처음 3개 파라미터를 제외한 나머지 4개는 선택이므로 아래와 같이 처리.
				int numMeter = SHELL_decimal(cmdArgs[0]); /* num of meter */
				int type1 = SHELL_decimal(cmdArgs[1]);
				int port1 = SHELL_decimal(cmdArgs[2]);
				int type2 = (numOfCmdArgs >= 4) ? SHELL_decimal(cmdArgs[3]) : 0xFF;
				int port2 = (numOfCmdArgs >= 5) ? SHELL_decimal(cmdArgs[4]) : 0xFF;
				int type3 = (numOfCmdArgs >= 6) ? SHELL_decimal(cmdArgs[5]) : 0xFF;
				int port3 = (numOfCmdArgs >= 7) ? SHELL_decimal(cmdArgs[6]) : 0xFF;
				if (cmdIdx == CMDN_PWMETER) {
					TEST_pwmeter(numMeter, type1, port1, type2, port2, type3,
						     port3);
				} else if (cmdIdx == CMDN_PMETER) {
					TEST_pmeter(numMeter, type1, port1, type2, port2, type3,
						    port3);
				} else {
					goto SHELL_CMD_ERROR;
				}
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;

		case CMDN_PMODEM:
			TEST_pmodem();
			break;

		case CMDN_PNFC:
			TEST_checkNFC();
			break;

		case CMDN_RNFC:
			TEST_resetNFC();
			break;

		case CMDN_PWSN:
			if (numOfCmdArgs >= 3) {
				char sn[LEN_ARG_STRING];
				SHELL_string(cmdArgs[0], sn); /* serial num */
				int ri = SHELL_decimal(cmdArgs[1]); /* ri */
				int mi = SHELL_decimal(cmdArgs[2]); /* mi */
				TEST_pwsn(sn, ri, mi);
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;

		case CMDN_PMWSN:
			if (numOfCmdArgs >= 9) {
				char sn[LEN_ARG_STRING];
				SHELL_string(cmdArgs[0], sn); /* serial num */
				int ri = SHELL_decimal(cmdArgs[1]); /* ri */
				int mi = SHELL_decimal(cmdArgs[2]); /* mi */
				int caliber = SHELL_decimal(cmdArgs[3]); /* Meter caliber */
				int q3 = SHELL_decimal(cmdArgs[4]); /* Q3 */
				int qt = SHELL_decimal(cmdArgs[5]); /* Qt */
				int q2 = SHELL_decimal(cmdArgs[6]); /* Q2 */
				int q1 = SHELL_decimal(cmdArgs[7]); /* Q1 */
				int marker = SHELL_decimal(cmdArgs[8]); /* Maker */
				TEST_pmwsn(sn, ri, mi, caliber, q3, qt, q2, q1, marker);
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;

		case CMDN_PRSN:
			TEST_prsn();
			break;

		case CMDN_PMRSN:
			TEST_pmrsn(TRUE);
			break;

		case CMDN_WMETER:
			if (numOfCmdArgs >= 1) {
				char meterValue[LEN_ARG_STRING];
				SHELL_string(cmdArgs[0], meterValue); /* value */
				TEST_writeMeter(meterValue);
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;

#if LORA_DEVICE
		case CMDN_PWAEUI:
			if (numOfCmdArgs >= 1) {
				char appEui[LEN_ARG_STRING];
				SHELL_string(cmdArgs[0], appEui); /* application EUI */
				TEST_pwaeui(appEui);
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;

		case CMDN_PRAEUI:
			TEST_praeui();
			break;

		case CMDN_PWDEUI:
			if (numOfCmdArgs >= 1) {
				char devEui[LEN_ARG_STRING];
				SHELL_string(cmdArgs[0], devEui); /* device EUI */
				TEST_pwdeui(devEui);
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;

		case CMDN_PRDEUI:
			TEST_prdeui();
			break;

		case CMDN_PWAKEY:
			if (numOfCmdArgs >= 1) {
				char appKey[LEN_ARG_STRING];
				SHELL_string(cmdArgs[0], appKey); /* device EUI */
				TEST_pwakey(appKey);
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;

		case CMDN_PRAKEY:
			TEST_prakey();
			break;

#else // NBIOT_DEVICE

		case CMDN_PRNBID:
			TEST_prnbid();
			break;

		case CMDN_PWSERVER:
			if (numOfCmdArgs >= 3) {
				char ip[LEN_ARG_STRING];
				char sc[LEN_ARG_STRING];
				SHELL_string(cmdArgs[0], ip); /* server ip */
				int port = SHELL_decimal(cmdArgs[1]); /* server port */
				SHELL_string(cmdArgs[2], sc); /* service code */
				TEST_pwserver(ip, port, sc);
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;

		case CMDN_PRSERVER:
			TEST_prserver();
			break;

		case CMDN_PWFOTA:
			if (numOfCmdArgs >= 3) {
				char ip[LEN_ARG_STRING];
				SHELL_string(cmdArgs[0], ip); /* fota server ip */
				int port = SHELL_decimal(cmdArgs[1]); /* fota port */
				int interval = SHELL_decimal(cmdArgs[2]); /* fota interval */
				TEST_pwfota(ip, port, interval);
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;

		case CMDN_PRFOTA:
			TEST_prfota();
			break;

		case CMDN_PNBCTX:
			TEST_pnbctx();
			break;
#endif

#if defined(AUX_REPEATER)
		case CMDN_PCTX:
			if (numOfCmdArgs >= 1) {
				int ch = SHELL_decimal(cmdArgs[0]); /* channel */
				int txLevel = (numOfCmdArgs >= 2) ? SHELL_decimal(cmdArgs[1]) : 0;
				TEST_ContinuousTx(ch, txLevel, 1);
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;

		case CMDN_PTX:
			if (numOfCmdArgs >= 1) {
				int ch = SHELL_decimal(cmdArgs[0]); /* channel */
				int txLevel = (numOfCmdArgs >= 2) ? SHELL_decimal(cmdArgs[1]) : 0;
				int msgLen = (numOfCmdArgs >= 3) ? SHELL_decimal(cmdArgs[2]) : 60;
				TEST_TxData(ch, txLevel, msgLen, 1);
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;

		case CMDN_PRX:
			if (numOfCmdArgs >= 1) {
				int ch = SHELL_decimal(cmdArgs[0]); /* channel */
				TEST_RxData(ch, 1);
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;

		case CMDN_RID:
			TEST_readIDs();
			break;

		case CMDN_WID:
			if (numOfCmdArgs >= 3) {
				char panId[LEN_ARG_STRING];
				char nwkId[LEN_ARG_STRING];
				SHELL_string(cmdArgs[0], panId); /* pan id */
				SHELL_string(cmdArgs[1], nwkId); /* nwk id */
				int slaveId = SHELL_decimal(cmdArgs[2]); /* slave ID */
				TEST_writeIDs(panId, nwkId, slaveId);
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;

		case CMDN_RMSG:
			if (numOfCmdArgs >= 1) {
				int ch = SHELL_decimal(cmdArgs[0]); /* channel */
				TEST_RxMsg(ch);
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;

		case CMDN_SCAN:
			if (numOfCmdArgs >= 1) {
				int ch = SHELL_decimal(cmdArgs[0]); /* channel */
				if (ch < 1 || ch > 4) {
					goto SHELL_CMD_ERROR;
				}
				TEST_channelScan(ch);
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;

		case CMDN_RSSI:
			if (numOfCmdArgs >= 1) {
				int ch = SHELL_decimal(cmdArgs[0]); /* channel */
				TEST_checkRSSI(ch);
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;

		case CMDN_POWER:
			if (numOfCmdArgs >= 1) {
				int ch = SHELL_decimal(cmdArgs[0]); /* channel */
				TEST_checkRxPower(ch);
			} else {
				goto SHELL_CMD_ERROR;
			}
			break;
#else
		case CMDN_PCTX:
		case CMDN_PTX:
		case CMDN_PRX:
		case CMDN_RID:
		case CMDN_WID:
		case CMDN_RMSG:
		case CMDN_SCAN:
		case CMDN_RSSI:
		case CMDN_POWER:
			printf("Not supported - command only for Aux Repeater\n");
			break;
#endif // #if defined(AUX_REPEATER)

		case CMDN_MRSN:
			TEST_pmrsn(FALSE);
			break;

		case CMDN_RI_CTRL:
			if (numOfCmdArgs >= 1) {
				int flag = SHELL_decimal(cmdArgs[0]);
				TEST_riCtrlMode(flag);
			} else {
				TEST_riCtrlMode(0xFF);
			}
			break;

		case CMDN_ES:
#if !defined(AUX_REPEATER)
			if (numOfCmdArgs >= 1) {
				dataFlash_eraseSector(cmdArgs[0]);
			} else {
				goto SHELL_CMD_ERROR;
			}
#endif
			break;

		case CMDN_GD:
#if !defined(AUX_REPEATER)
			if (numOfCmdArgs >= 1) {
				if (numOfCmdArgs >= 2) {
					dataFlash_genData(SHELL_decimal(cmdArgs[0]),
							  SHELL_decimal(cmdArgs[1]));
				} else {
					dataFlash_genData(SHELL_decimal(cmdArgs[0]), 0);
				}
			} else {
				goto SHELL_CMD_ERROR;
			}
#endif
			break;

		case CMDN_MAP:
#if !defined(AUX_REPEATER)
			dataFlash_displayMapSector();
#endif
			break;

		case CMDN_DDS:
#if !defined(AUX_REPEATER)
			if (numOfCmdArgs >= 1) {
				dataFlash_displayDataSector(SHELL_decimal(cmdArgs[0]));
			} else {
				goto SHELL_CMD_ERROR;
			}
#endif
			break;

		case CMDN_RD:
#if !defined(AUX_REPEATER)
			if (numOfCmdArgs >= 4) {
				dataFlash_readData(SHELL_decimal(cmdArgs[0]),
						   SHELL_decimal(cmdArgs[1]),
						   SHELL_decimal(cmdArgs[2]),
						   SHELL_decimal(cmdArgs[3]));
			} else {
				goto SHELL_CMD_ERROR;
			}
#endif
			break;

		default:
			printf("unknown command\n");
			break;
		SHELL_CMD_ERROR:
			printf("command error!\n");
			printf("usage: %s\n", CmdSet[cmdIdx].usage);
		}
	}
}

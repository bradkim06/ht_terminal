#include "app.h"
#include "check_meter_misc.h"
#include "intrinsics.h"
#include "modem.h"
#include "osal.h"
#include "osal_Timer.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <msp430.h>
#include "common_header.h"
#include "fota.h"
#include "uart.h"
#include "flashDriver.h"
#include "meter.h"

#define CONNECT_SERVER 1
#define SEND_FW_VER 2
#define SOCK_CLOSE 3

#define MASS_ERASE 1
#define SECT_ERASE 2

#define SEND_READY 1
#define SEND_ACK 2
#define SEND_REPEAT 3
#define SEND_FINISH 4

#define FOTA_DELAY 100

FotaStatus_t Fota;

char rxBuf[RX_MAX_LEN];
int rxLen;

static volatile unsigned long address = 0;

static int checkFlashBusy()
{
	while (1) {
		if ((FCTL3 & BUSY) == 0) {
			return 1;
		}
	}
}

static void eraseCode(unsigned long addr, int option)
{
	if (checkFlashBusy()) {
		FCTL3 = FWKEY;
		if (option == SECT_ERASE) {
			FCTL1 = FWKEY + ERASE; // Set Erase bit
		} else if (option == MASS_ERASE) {
			FCTL1 = FWKEY + MERAS; // Set Erase bit
		}

		if (checkFlashBusy()) {
			__data20_write_char(addr, 0);
		}

		asm(" bis.w #0,R3 "); // Flash contents: 0x03 0xd3
		asm(" bis.w #0,R3 "); // Flash contents: 0x03 0xd3
		asm(" bis.w #0,R3 "); // Flash contents: 0x03 0xd3

		if (checkFlashBusy()) {
			FCTL1 = FWKEY; // Set Erase bit
			FCTL3 = FWKEY + LOCK;
		}
	}
	checkFlashBusy();
}

static void writeCode(unsigned long addr, volatile unsigned char data[], int size)
{
	if (checkFlashBusy()) {
		FCTL3 = FWKEY; // clear lock
		FCTL1 = FWKEY + WRT; // Set Write bit

		for (int i = 0; i < size; i++) {
			if (checkFlashBusy()) {
				__data20_write_char(addr + i, data[i]);
			}
		}

		if (checkFlashBusy()) {
			FCTL1 = FWKEY; // Set Erase bit
			FCTL3 = FWKEY + LOCK;
		}
	}

	checkFlashBusy();
}

#ifdef FOTA_DEBUG
static void fotaPrint(char *str)
{
	while (*str != 0) {
		while (!(UCTXIFG & UCA1IFG))
			; //Ensure that transmit interrupt flag is set
		UCA1TXBUF = *str++; //Load UCA0TXBUF with current string element
	}
}
#endif

static void modemRxClear()
{
	for (int i = 0; i < sizeof(rxBuf); i++) {
		rxBuf[i] = 0;
	}
	rxLen = 0;
}

static void modemSend(char *str)
{
	modemRxClear();
	while (*str != 0) {
		while (!(UCTXIFG & UCA2IFG))
			; //Ensure that transmit interrupt flag is set
		UCA2TXBUF = *str++; //Load UCA0TXBUF with current string element
	}
}

#ifdef FOTA_DEBUG
static void fotaModemPrint(char *p)
{
	fotaPrint("[RX] ");
	for (int i = 0; i < strlen(p); i++) {
		char ch = *(p + i);
		if (ch == '\r' || ch == '\n') {
			fotaPrint(".");
		} else {
			fotaPrint(&ch);
		}
	}
	fotaPrint("\n\r");
}
#endif

static void fotaSend(char *str)
{
	modemSend(str);
#ifdef FOTA_DEBUG
	fotaPrint("[TX] ");
	fotaPrint(str);
#endif
}

int testatoi(char *cdata)
{
	int data = 0;

	while (*cdata) {
		if (*cdata >= '0' && *cdata <= '9') {
			if (data != 0) {
				int mulData = data;
				for (int i = 0; i < 9; i++) {
					data += mulData;
				}
			}
			data = data + *cdata;
			data = data - '0';
		} else {
			return data;
		}

		cdata++;
	}

	return data;
}

static void fotaRecv()
{
#define AT_MODEM_RX "+NSONMI:"
#define AT_DFOTA_PORT "18099,"

	while (1) {
		volatile int len = 0;
		volatile int count = 0;
		for (; count < 600; count++) {
			MISC_delayMs(FOTA_DELAY);
			if (len != rxLen) {
				len = rxLen;
			} else if (len > 5) {
				break;
			}
		}

		if (len > 0) {
			char *p = NULL;

			if ((p = strstr(rxBuf, AT_MODEM_RX))) {
#ifdef FOTA_DEBUG
				fotaModemPrint(rxBuf);
#endif
				fotaSend("AT+NSORF=1,1358\n\r");
			} else if ((p = strstr(rxBuf, AT_DFOTA_PORT))) {
				// data len
				p = strstr(p, ",") + 1;
				int payloadLen = testatoi(p);

				// payload
				p = strstr(p, ",") + 1;
				volatile unsigned char rxData[514];
				for (int i = 0; i < payloadLen; i++) {
					rxData[i] = ascii2BCD(*(p + 0), *(p + 1));
					p += 2;
				}

				unsigned char checksum =
					std_checksum((unsigned char *)rxData, payloadLen - 1);

				if (rxData[payloadLen - 1] == checksum) {
					if (rxData[0] == 0xB1) {
						send(SEND_ACK);
						eraseCode(address, SECT_ERASE);
						writeCode(address, &rxData[1], payloadLen - 2);
						address += 512;
					} else if (rxData[0] == 0xB2) {
						send(SEND_ACK);
						address = 0;
						for (int i = 1; i < payloadLen - 1; i++) {
							if (address) {
								unsigned long mulData = address;
								for (int j = 0; j < 255; j++) {
									address += mulData;
								}
							}
							address |= rxData[i];
						}
					} else if (rxData[0] == 0xB3) {
						send(SEND_FINISH);
#ifdef FOTA_DEBUG
						fotaPrint("FOTA Finish\n\r");
#endif
						__disable_interrupt();
						eraseCode(address, SECT_ERASE);
						writeCode(address, &rxData[1], payloadLen - 2);
						MISC_delayMs(5000);
						PMMCTL0 = (PMMPW + PMMSWPOR);
					}
				} else {
					send(SEND_REPEAT);
				}
			} else {
#ifdef FOTA_DEBUG
				fotaModemPrint(rxBuf);
#endif
				modemRxClear();
			}
		} else {
			send(SEND_REPEAT);
		}
	}
}

static void send(int option)
{
	if (option == SEND_READY) {
		fotaSend("AT+NSOSD=1,6,726561647915\n\r");
	} else if (option == SEND_ACK) {
		fotaSend("AT+NSOSD=1,4,61636B2F\n\r");
	} else if (option == SEND_REPEAT) {
		fotaSend("AT+NSOSD=1,7,72657065617481\n\r");
	} else if (option == SEND_FINISH) {
		fotaSend("AT+NSOSD=1,7,66696E69736881\n\r");
	}
}

void startFota()
{
	Fota.Status = 1;

#ifdef FOTA_DEBUG
	char *str = "fota start, send Ready\n\r";
	fotaPrint(str);
#endif
	// fotaSend("AT+NATSPEED=115200,3,1,2,1\n\r");
	send(SEND_READY);

	fotaRecv();
}

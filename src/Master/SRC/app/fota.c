#include "check_meter_misc.h"
#include "intrinsics.h"
#include "modem.h"
#include "osal.h"
#include "osal_Timer.h"
#include <msp430.h>
#include "common_header.h"
#include "fota.h"
#include "uart.h"

#define CONNECT_SERVER 1
#define SEND_FW_VER 2
#define SOCK_CLOSE 3

#define MASS_ERASE 1
#define SECT_ERASE 2

int fotaStep = 0;
int fotaStatus = 0;

char rxBuf[512];
int rxLen;

static int checkFlashBusy()
{
	for (int i = 0; i < 800000; i++) {
		if ((FCTL3 & BUSY) == 0) {
			return 1;
		}
	}

	return 0;
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
}

char test_data[74] = { 0x81, 0x00, 0x00, 0x5C, 0xB1, 0x13, 0x22, 0xCE, 0x0C, 0x93, 0x02, 0x24, 0xB1,
		       0x13, 0x1C, 0x9A, 0x0C, 0x43, 0xB0, 0x13, 0x00, 0x5C, 0x1C, 0x43, 0xB1, 0x13,
		       0x10, 0xCE, 0x84, 0x20, 0xB2, 0x1F, 0x1E, 0x20, 0xCC, 0x1C, 0xE4, 0x20, 0x12,
		       0x1E, 0x30, 0x1F, 0xE4, 0x20, 0xA8, 0x1E, 0xE4, 0x20, 0xE4, 0x20, 0x00, 0x1C,
		       0xE4, 0x20, 0xE4, 0x20, 0xE4, 0x20, 0xE4, 0x20, 0x7C, 0x1D, 0xE4, 0x20, 0xE4,
		       0x20, 0xE4, 0x20, 0xBC, 0x20, 0xE4, 0x20, 0xB6, 0xFF };

static void writeCode(unsigned long addr)
{
	if (checkFlashBusy()) {
		FCTL3 = FWKEY; // clear lock
		FCTL1 = FWKEY + WRT; // Set Write bit

		for (int i = 0; i < sizeof(test_data); i++) {
			if (checkFlashBusy()) {
				__data20_write_char(addr + i, *(test_data + i));
			}
		}

		if (checkFlashBusy()) {
			FCTL1 = FWKEY; // Set Erase bit
			FCTL3 = FWKEY + LOCK;
		}
	}
}

static void fotaPrint(char *str)
{
	while (*str != 0) {
		while (!(UCTXIFG & UCA1IFG))
			; //Ensure that transmit interrupt flag is set
		UCA1TXBUF = *str++; //Load UCA0TXBUF with current string element
	}
}

static void modemSend(char *str)
{
	for (int i = 0; i < sizeof(rxBuf); i++) {
		rxBuf[i] = 0;
	}
	rxLen = 0;

	while (*str != 0) {
		while (!(UCTXIFG & UCA2IFG))
			; //Ensure that transmit interrupt flag is set
		UCA2TXBUF = *str++; //Load UCA0TXBUF with current string element
	}
}

static void fotaModemPrint(char *p, int len)
{
	for (int i = 0; i < len; i++) {
		char ch = *(p + i);
		if (ch == '\r' || ch == '\n') {
			fotaPrint(".");
		} else {
			fotaPrint(&ch);
		}
	}
	fotaPrint("\n\r");
}

static void fotaSend(char *str)
{
	fotaPrint(str);
	fotaPrint("\n\r");

	modemSend(str);
}

static void fotaRecv()
{
	int len = 0;

	for (int i = 0; i < 20; i++) {
		MISC_delayMs(500);
		if (len != rxLen) {
			len = rxLen;
		} else {
			break;
		}
	}

	if (len > 0) {
		fotaModemPrint(rxBuf, len);
	} else {
		char *str = "no modem response\n\r";
		fotaPrint(str);
	}
}

void startFota()
{
	fotaStatus = 1;
	WDTCTL = WDTPW | WDTHOLD;
	__disable_interrupt();

	eraseCode(0x5C00, MASS_ERASE);
	writeCode(0xffb6);
	eraseCode(0x10000, MASS_ERASE);
	eraseCode(0x20000, MASS_ERASE);

	__enable_interrupt();

	char *str = "fota start\n\r";
	fotaPrint(str);

	fotaSend("AT+NSOSD=1,4,A1323334\n\r");
	fotaRecv();

	fotaSend("AT+NSOCL=1\n\r");
	fotaRecv();
	fotaStep = CONNECT_SERVER;
	while (1)
		;
}

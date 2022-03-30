#include <msp430.h>
#include "common_header.h"

int fotaStatus = 0;

static int checkFlashBusy()
{
	for (int i = 0; i < 10000; i++) {
		if ((FCTL3 & BUSY) == 0) {
			return 1;
		}
	}

	return 0;
}

static void eraseCode(unsigned long addr)
{
	WDTCTL = WDTPW | WDTHOLD;
	halIntState_t intState;
	HAL_ENTER_CRITICAL_SECTION(intState);

	FCTL3 = FWKEY;
	FCTL1 = FWKEY + ERASE; // Set Erase bit

	__data20_write_char(addr, 0);

	asm(" bis.w #0,R3 "); // Flash contents: 0x03 0xd3
	asm(" bis.w #0,R3 "); // Flash contents: 0x03 0xd3
	asm(" bis.w #0,R3 "); // Flash contents: 0x03 0xd3

	FCTL1 = FWKEY; // Set Erase bit
	FCTL3 = FWKEY + LOCK;

	HAL_EXIT_CRITICAL_SECTION(intState);
}

static void writeCode(unsigned long addr)
{
	WDTCTL = WDTPW | WDTHOLD;
	halIntState_t intState;
	HAL_ENTER_CRITICAL_SECTION(intState);

	FCTL3 = FWKEY; // clear lock
	FCTL1 = FWKEY + WRT; // Set Write bit

	/* for (int i = 0; i < sizeof(test_data); i++) { */
	/* 	for (int i = 0; i < 10000; i++) { */
	/* 		if ((FCTL3 & BUSY) == 0) { */
	/* 			break; */
	/* 		} */
	/* 	} */
	/* 	__data20_write_char(address + i, *(test_data + i)); */
	/* } */

	FCTL1 = FWKEY; // Set Erase bit
	FCTL3 = FWKEY + LOCK;

	HAL_EXIT_CRITICAL_SECTION(intState);
}

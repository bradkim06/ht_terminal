#include "lcdDriver.h"
#include "common_header.h"
#include "check_meter_misc.h"

int MISC_getBslType()
{
	int type = TERM_BSL_ENABLE;

	// check BSL board
	P8DIR &= ~0x10;
	P8REN |= 0x10;
	P8OUT |= 0x10;

	MISC_delayUs(10);
	if (P8IN & 0x10) {
		type = TERM_BSL_DISABLE;
	}

	P8REN &= ~0x10;
	P8OUT &= ~0x10;

	return type;
}

int MISC_findPushButton()
{
	int found = 1;

	// check push button
	P8DIR &= ~0x08;
	P8REN |= 0x08;
	P8OUT |= 0x08;

	MISC_delayUs(10);
	if (P8IN & 0x08) {
		found = 0;
	}

	P8REN &= ~0x08;
	P8DIR |= 0x08;
	P8OUT &= ~0x08;

	return found;
}

int MISC_NfcPortSelect(void)
{
	int port = 7;
	// check BSL board
	P8DIR &= ~0x10;
	P8REN |= 0x10;
	P8OUT |= 0x10;

	MISC_delayUs(10);

	if (P8IN & 0x10) {
		port = 2;
	}

	P8REN &= ~0x10;
	P8OUT &= ~0x10;

	return port;
}

int MISC_getDeviceType()
{
#if defined(AUX_REPEATER)
	return TERM_MODEL_HAT_435W;
#else
	int type = TERM_MODEL_UNKNOWN;
	if (LCD_Exist()) {
		type = TERM_MODEL_HAT_114W;
	} else {
		P8DIR &= ~0x02;
		P8REN |= 0x02;
		P8OUT |= 0x02;

		MISC_delayUs(10);
		if (P8IN & 0x02) {
			type = TERM_MODEL_HAT_124W;
		} else {
			type = TERM_MODEL_HTM_115W;
		}

		P8REN &= ~0x02;
		P8OUT &= ~0x02;
	}
	return type;
#endif
}

#pragma optimize = none
void MISC_delayUs(uint32 timeout)
{
	// This sequence uses exactly 8 clock cycle for each round (1 micro sec)
	do {
		NOP();
		NOP();
		NOP();
		NOP();
	} while (--timeout);
} // MISC_delayUs

#pragma optimize = none
void MISC_delayMs(uint32 timeout)
{
	unsigned int i;
	// This sequence uses exactly 8000 clock cycle for each round (1ms)
	do {
		i = 1000;
		do {
			NOP();
			NOP();
			NOP();
			NOP();
		} while (--i);
	} while (--timeout);
} // MISC_delayMs

/////////////////////////////////////////////////////////////////
// Big-Endian, Little-Endian
/////////////////////////////////////////////////////////////////
// uint16 swaps(uint16 value16)
// {
//     uint16 n=1;
//     if(*(uint8 *)&n == 1){ // littel-endian
//         n = ((value16 << 8) & 0xFF00);
//         n |= ((value16 >> 8) & 0x00FF);
//         return n;
//     }
//     return value16;
// }

// uint32 swapl(uint32 value32)
// {
//     uint32 n=1;
//     if(*(uint8 *)&n == 1){ // littel-endian

//         n = ((value32 << 24) & 0xFF000000);
//         n = ((value32 << 8) & 0x00FF0000);
//         n = ((value32 >> 8) & 0x0000FF00);
//         n = ((value32 >> 24) & 0x000000FF);
//         return n;
//     }
//     return value32;
// }

// uint16 htons(uint16 value16)
// {
//     return swaps(value16);
// }

// uint32 htonl(uint32 value32)
// {
//     return swapl(value32);
// }

// uint16 ntohs(uint16 value16)
// {
//     return swaps(value16);
// }

// uint32 ntohl(uint32 value32)
// {
//     return swapl(value32);
// }

static uint16 ShiftReg;
static uint16 Mask;

/* Initialize the seed from the ID of the node */
BOOL MISC_initRandom(uint32 myAddr)
{
	ShiftReg = 119 * 119 * ((myAddr & 0xFFFF) + 1);
	//initSeed = ShiftReg;
	Mask = 137 * 29 * ((myAddr & 0xFFFF) + 1);

	return SUCCESS;
}

/* Return the next 16 bit random number */
uint16 MISC_getRandomNum()
{
	BOOL endbit;
	uint16 tmpShiftReg;

	tmpShiftReg = ((ShiftReg + RTCPS) & 0xFFFF);
	endbit = ((tmpShiftReg & 0x8000) != 0);
	tmpShiftReg <<= 1;
	if (endbit)
		tmpShiftReg ^= 0x100b;
	tmpShiftReg++;
	ShiftReg = tmpShiftReg;
	tmpShiftReg = tmpShiftReg ^ Mask;

	return tmpShiftReg;
}

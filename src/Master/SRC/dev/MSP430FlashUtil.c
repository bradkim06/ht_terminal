

#include <msp430.h>
#include "check_meter_misc.h"
#include "uart.h"
#include "common_header.h"
#include "MSP430FlashUtil.h"

#define STATIC static

/**
 * @brief Flash sector size in MSP430
 */
#define FLASH_SECTOR_LENGTH (uint32)128
/**
 * @brief 1byte write timeout in Flash memory
 *        (35cycle / 333KhZ = 105 usec, about double for extra -> 200 usec)
 */
#define FLASH_BYTE_PROGRAM_USEC (uint32)200
/**
 * @brief sector(=128B) write timeout in Flash memory
 */
#define FLASH_SECTOR_PROGRAM_USEC (uint32)(FLASH_BYTE_PROGRAM_USEC * FLASH_SECTOR_LENGTH)

// Function prototypes
STATIC void initFlash(void);
STATIC void doneFlash(void);

#pragma inline
STATIC BOOL isBusyFlash(uint32 timeoutUsec)
{
	if (FCTL3 & BUSY) {
		MISC_delayUs(timeoutUsec);
		return (FCTL3 & BUSY);
	}

	return FALSE;
}

STATIC void initFlash(void)
{
#if defined FCTL2
	FCTL2 = FWKEY + FSSEL_1 + FN4 + FN3; // Flash Timing Generator as MCLK/24 -> 333 kHz.
#endif
}

STATIC void doneFlash()
{
	return;
}

BOOL MSP430FLASH_erasePage(unsigned char *addr)
{
	initFlash();

	if (isBusyFlash(FLASH_SECTOR_PROGRAM_USEC)) {
		return FALSE;
	}

	FCTL1 = FWKEY + ERASE; // Set Erase bit

	if (addr == (unsigned char *)FLASH_SEGA_ADDR) {
		if (FCTL3 & LOCKA) //Test LOCKA
			FCTL3 = FWKEY + LOCKA; // Clear Lock, unlock SegmentA bits
		else
			FCTL3 = FWKEY; // Set LOCK bit
	} else {
		FCTL3 = FWKEY; // Clear Lock bit
	}

	*addr = 0; // Dummy write to erase Flash segment

	BOOL result = TRUE;
	if (isBusyFlash(FLASH_SECTOR_PROGRAM_USEC)) {
		result = FALSE;
	}

	FCTL1 = FWKEY; // Clear ERASE bit

	if (addr == (unsigned char *)FLASH_SEGA_ADDR) {
		if (FCTL3 & LOCKA) //Test LOCKA
			FCTL3 = FWKEY + LOCK; // Set LOCK bit
		else
			FCTL3 = FWKEY + LOCK + LOCKA; // Set LOCK, lock SegmentA bit
	} else {
		FCTL3 = FWKEY + LOCK; // Set LOCK bit
	}

	doneFlash();

	return result;
}

BOOL MSP430FLASH_read(unsigned char *addr, unsigned char *value, unsigned short len)
{
	unsigned char *flashPtr = (unsigned char *)addr; // Initialize Flash pointer
	for (unsigned int i = 0; i < len; i++) {
		*(value + i) = *(flashPtr + i); // Write value to flash
	}
	return TRUE;
}

BOOL MSP430FLASH_write(unsigned char *addr, unsigned char *buf, unsigned short len)
{
	initFlash();

	// Check busy befor operating
	if (isBusyFlash(FLASH_SECTOR_PROGRAM_USEC)) {
		return FALSE;
	}

	if (addr == (unsigned char *)FLASH_SEGA_ADDR) {
		if (FCTL3 & LOCKA) //Test LOCKA
			FCTL3 = FWKEY + LOCKA; // Clear Lock, unlock SegmentA bits
		else
			FCTL3 = FWKEY; // Set LOCK bit
	} else {
		FCTL3 = FWKEY; // Clear Lock bit
	}

	FCTL1 = FWKEY + WRT; // Set WRT bit for write operation

	BOOL result = TRUE;
	for (int i = 0; i < len; i++) {
		*(addr + i) = *(buf + i); // Write value to flash

		// Busy check
		if (isBusyFlash(FLASH_BYTE_PROGRAM_USEC)) {
			result = FALSE;
			break;
		}

		// Verify data
		if (*(addr + i) != *(buf + i)) {
			result = FALSE;
			break;
		}

		// Voltage changed during program error.
		if (FCTL4 & VPE) {
			FCTL4 &= ~(VPE); // clear VPE
			result = FALSE;
			break;
		}
	}

	FCTL1 = FWKEY; // Clear WRT bit

	if (addr == (unsigned char *)FLASH_SEGA_ADDR) {
		if (FCTL3 & LOCKA) //Test LOCKA
			FCTL3 = FWKEY + LOCK; // Set LOCK bit
		else
			FCTL3 = FWKEY + LOCK + LOCKA; // Set LOCK, lock SegmentA bit
	} else {
		FCTL3 = FWKEY + LOCK; // Set LOCK bit
	}

	doneFlash();

	return result;
}

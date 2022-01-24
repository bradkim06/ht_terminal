#ifndef __MSP430FLASHUTIL_H__
#define __MSP430FLASHUTIL_H__

BOOL MSP430FLASH_erasePage(unsigned char *addr);
BOOL MSP430FLASH_read(unsigned char *addr, unsigned char *value, unsigned short len);
BOOL MSP430FLASH_write(unsigned char *addr, unsigned char *buf, unsigned short len);

#endif
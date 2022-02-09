#ifndef __CHECK_METER_MISC_H__
#define __CHECK_METER_MISC_H__

#include "common_header.h"

int MISC_getDeviceType(void);
int MISC_getBslType();
int MISC_findPushButton();
int MISC_NfcPortSelect(void);
void MISC_delayUs(uint32 timeout);
void MISC_delayMs(uint32 timeout);

//char* misc_itoa( int value, char *string, char radix );
//char* misc_utoa( int value, char *digits, char base );

// uint8* itoa( int16 value, uint8 *string, uint8 radix );
// uint8* utoa( uint16 value, uint8 *digits, int8 base );

/* Initialize the seed from the ID of the node */
BOOL MISC_initRandom(uint32 myAddr);
/* Return the next 16 bit random number */
uint16 MISC_getRandomNum();

#endif // __CHECK_METER_MISC_H__

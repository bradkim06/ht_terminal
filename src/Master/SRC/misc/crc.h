//===================================================================
//
// $Id: crc.h, v 1.0  2009-01-28 ¿ÀÈÄ 7:56:19 $
//
//===================================================================
// Copyright
//===================================================================
#ifndef _CRC_H
#define _CRC_H

#include "common_header.h"

uint16 CRC16_ansi(uint8 *data, uint16 len);
uint16 CRC16_ccitt(uint8 *data, uint16 len);
uint16 CRC16_ccittByte(uint16 fcs, uint8 c);

#endif

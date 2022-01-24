//******************************************************************************
//! @file       CC1200_rx_sniff_mode_reg_config.h
//! @brief      CC1120 register export from SmartRF Studio
//
//  Copyright (C) 2013 Texas Instruments Incorporated - http://www.ti.com/
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions
//  are met:
//
//      Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//
//      Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//      Neither the name of Texas Instruments Incorporated nor the names of
//      its contributors may be used to endorse or promote products derived
//      from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//*****************************************************************************/

#ifndef CC1200_RX_SNIFF_MODE_REG_CONFIG_H
#define CC1200_RX_SNIFF_MODE_REG_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * INCLUDES
 */
#include "hal_spi_rf_trxeb.h"
#include "cc120x_spi.h"

/******************************************************************************
 *            국내용 424MHz
 ******************************************************************************
    - RX filter BW: 12.5KHz
    - Modulation: 2-GFSK
    - Deviation: 2.4kHz
 */

typedef struct {
	uint16 addr;
	uint8 data;
} SpiRegisterSetting_t;

static const SpiRegisterSetting_t PreferredSettings424[] = {
	{ CC120X_IOCFG2, 0x11 }, // RSSI Check GPIO2
	{ CC120X_IOCFG0, 0x06 }, // Rx Interrupt GPIO0
	{ CC120X_SYNC3, 0xA6 },
	{ CC120X_SYNC2, 0x5A },
	{ CC120X_SYNC1, 0x96 },
	{ CC120X_SYNC0, 0x56 },
	{ CC120X_SYNC_CFG1, 0xA8 },
	{ CC120X_DEVIATION_M, 0x7B },
	{ CC120X_MODCFG_DEV_E, 0x08 },
	{ CC120X_DCFILT_CFG, 0x5D },
	{ CC120X_PREAMBLE_CFG1, 0x37 }, //0x36
	{ CC120X_PREAMBLE_CFG0,
	  0xAA }, //yikim 2018.02.01 0x8A: 11symbols-6ms(93%), 0xCA : 16 symbols-8ms, 0xAA : 13 symbols-7ms(99%), 0x9A : 12 symbols-6.5ms(96%)
	{ CC120X_IQIC, 0xCB },
	{ CC120X_CHAN_BW, 0xA1 },
	{ CC120X_MDMCFG1, 0x60 },
	{ CC120X_MDMCFG0, 0x05 },
	{ CC120X_SYMBOL_RATE2, 0x4F },
	{ CC120X_SYMBOL_RATE1, 0x75 },
	{ CC120X_SYMBOL_RATE0, 0x10 },
	{ CC120X_AGC_REF, 0x30 },
	{ CC120X_AGC_CS_THR, 0xE0 }, // 0xFB -5 // 0xF9 -7 //0x0A - +9
	{ CC120X_AGC_CFG1, 0x40 },
	{ CC120X_AGC_CFG0, 0x83 },
	{ CC120X_FIFO_CFG, 0x00 },
	{ CC120X_SETTLING_CFG, 0x03 },
	{ CC120X_FS_CFG, 0x04 },
	{ CC120X_WOR_CFG1, 0x48 }, // 0x08 - High resolution // 0x48 - Medium high
	{ CC120X_WOR_CFG0, 0x20 },
	{ CC120X_WOR_EVENT0_MSB, 0x02 },
	{ CC120X_WOR_EVENT0_LSB, 0xff },
	{ CC120X_PKT_CFG2, 0x00 },
	{ CC120X_PKT_CFG1, 0x01 },
	{ CC120X_PKT_CFG0,
	  0x00 }, //0x20 - Variable packet length mode // 0x00 - Fixed packet length mode
	{ CC120X_RFEND_CFG0, 0x04 }, //0x01 - CS // 0x04 - PQT Detect
	{ CC120X_PA_CFG1, 0x3F },
	{ CC120X_PKT_LEN, 0x80 }, // 0x78 = 120bytes // 0x5A - 90byte
	{ CC120X_IF_MIX_CFG, 0x1C },
	{ CC120X_FREQOFF_CFG, 0x22 },
	{ CC120X_MDMCFG2, 0x0C },
	{ CC120X_FREQ2, 0x57 },
	{ CC120X_FREQ1, 0x1E },
	{ CC120X_FREQ0, 0x32 },
	{ CC120X_IF_ADC1, 0xEE },
	{ CC120X_IF_ADC0, 0x10 },
	{ CC120X_FS_DIG1, 0x07 },
	{ CC120X_FS_DIG0, 0xAF },
	{ CC120X_FS_CAL1, 0x40 },
	{ CC120X_FS_CAL0, 0x0E },
	{ CC120X_FS_DIVTWO, 0x03 },
	{ CC120X_FS_DSM0, 0x33 },
	{ CC120X_FS_DVC0, 0x17 },
	{ CC120X_FS_PFD, 0x00 },
	{ CC120X_FS_PRE, 0x6E },
	{ CC120X_FS_REG_DIV_CML, 0x1C },
	{ CC120X_FS_SPARE, 0xAC },
	{ CC120X_FS_VCO0, 0xB5 },
	{ CC120X_XOSC5, 0x0E },
};

//---------------------------------------------------------------+
//           424MHz  channel configuration(21 Channel)           |
//---------------------------------------------------------------+
//  ch0   424.7000MHz    ch1   424.7125MHz    ch2   424.7250MHz  |
//  ch3   424.7375MHz    ch4   424.7500MHz    ch5   424.7625MHz  |
//  ch6   424.7750MHz    ch7   424.7875MHz    ch8   424.8000MHz  |
//  ch9   424.8125MHz    ch10  424.8250MHz    ch11  424.8375MHz  |
//  ch12  424.8500MHz    ch13  424.8625MHz    ch14  424.8750MHz  |
//  ch15  424.8875MHz    ch16  424.9000MHz    ch17  424.9125MHz  |
//  ch18  424.9250MHz    ch19  424.9375MHz    ch20  424.9500MHz  |
//---------------------------------------------------------------+

//---------------------------------------------------------------+
//           447MHz  channel configuration(11 Channel)           |
//---------------------------------------------------------------+
//  ch21  447.8625MHz    ch22  447.8750MHz    ch23  447.8875MHz  |
//  ch24  447.9000MHz    ch25  447.9125MHz    ch26  447.9250MHz  |
//  ch27  447.9375MHz    ch28  447.9500MHz    ch29  447.9625MHz  |
//  ch30  447.9750MHz    ch31  447.9875MHz                       |
//---------------------------------------------------------------+

uint8 ChannelRF424[32][3] = {
	/* ch 00 */ { 0x54, 0xF0, 0xA4 },
	/* ch 01 */ { 0x54, 0xF1, 0x47 },
	/* ch 02 */ { 0x54, 0xF1, 0xEB },
	/* ch 03 */ { 0x54, 0xF2, 0x8F },
	/* ch 04 */ { 0x54, 0xF3, 0x33 },
	/* ch 05 */ { 0x54, 0xF3, 0xD7 },
	/* ch 06 */ { 0x54, 0xF4, 0x7A },
	/* ch 07 */ { 0x54, 0xF5, 0x1E },
	/* ch 08 */ { 0x54, 0xF5, 0xC2 },
	/* ch 09 */ { 0x54, 0xF6, 0x66 },
	/* ch 10 */ { 0x54, 0xF7, 0x0A },
	/* ch 11 */ { 0x54, 0xF7, 0xAE },
	/* ch 12 */ { 0x54, 0xF8, 0x52 },
	/* ch 13 */ { 0x54, 0xF8, 0xF5 },
	/* ch 14 */ { 0x54, 0xF9, 0x99 },
	/* ch 15 */ { 0x54, 0xFA, 0x3D },
	/* ch 16 */ { 0x54, 0xFA, 0xE1 },
	/* ch 17 */ { 0x54, 0xFB, 0x85 },
	/* ch 18 */ { 0x54, 0xFC, 0x28 },
	/* ch 19 */ { 0x54, 0xFC, 0xCC },
	/* ch 20 */ { 0x54, 0xFD, 0x70 },
	/* ch 21 */ { 0x59, 0x92, 0x8F },
	/* ch 22 */ { 0x59, 0x93, 0x33 },
	/* ch 23 */ { 0x59, 0x93, 0xD7 },
	/* ch 24 */ { 0x59, 0x94, 0x7A },
	/* ch 25 */ { 0x59, 0x95, 0x1E },
	/* ch 26 */ { 0x59, 0x95, 0xC2 },
	/* ch 27 */ { 0x59, 0x96, 0x66 },
	/* ch 28 */ { 0x59, 0x97, 0x0A },
	/* ch 29 */ { 0x59, 0x97, 0xAE },
	/* ch 30 */ { 0x59, 0x98, 0x52 },
	/* ch 31 */ { 0x59, 0x98, 0xF5 },
};

/******************************************************************************
 *            해외향 433MHz
 ******************************************************************************
    - RX filter BW: 22KHz
    - Modulation: 2-GFSK
    - Deviation: 6kHz
 */

// register 초기화 시 Deviation/Rx Fileter과 관련된 다음의 4개 register만 국내향과 달라짐
// CC120X_DEVIATION_M  = 0x39
// CC120X_MODCFG_DEV_E = 0x09
// CC120X_CHAN_BW      = 0x65
// CC120X_AGC_REF      = 0x32

//---------------------------------------------------------------+
//           433MHz  channel configuration(21 Channel)           |
//---------------------------------------------------------------+
//  ch0   433.050MHz     ch1   433.075MHz     ch2   433.100MHz   |
//  ch3   433.125MHz     ch4   433.150MHz     ch5   433.175MHz   |
//  ch6   433.200MHz     ch7   433.225MHz     ch8   433.250MHz   |
//  ch9   433.275MHz     ch10  433.300MHz     ch11  433.325MHz   |
//  ch12  433.350MHz     ch13  433.375MHz     ch14  433.400MHz   |
//  ch15  433.425MHz     ch16  433.450MHz     ch17  433.475MHz   |
//  ch18  433.500MHz     ch19  433.525MHz     ch20  433.550MHz   |
//---------------------------------------------------------------+

//---------------------------------------------------------------+
//    433MHz channel(for drive-by) configuration(11 Channel)     |
//---------------------------------------------------------------+
//  ch21  434.500MHz     ch22  434.525MHz     ch23  434.550MHz   |
//  ch24  434.575MHz     ch25  434.600MHz     ch26  434.625MHz   |
//  ch27  434.650MHz     ch28  434.675MHz     ch29  434.700MHz   |
//  ch30  434.725MHz     ch31  434.750MHz                        |
//---------------------------------------------------------------+

uint8 ChannelRf433[32][3] = {
	/* ch 00 */ { 0x56, 0x9C, 0x28 },
	/* ch 01 */ { 0x56, 0x9D, 0x70 },
	/* ch 02 */ { 0x56, 0x9E, 0xB8 },
	/* ch 03 */ { 0x56, 0x9F, 0xFF },
	/* ch 04 */ { 0x56, 0xA1, 0x47 },
	/* ch 05 */ { 0x56, 0xA2, 0x8F },
	/* ch 06 */ { 0x56, 0xA3, 0xD6 },
	/* ch 07 */ { 0x56, 0xA5, 0x1E },
	/* ch 08 */ { 0x56, 0xA6, 0x66 },
	/* ch 09 */ { 0x56, 0xA7, 0xAD },
	/* ch 10 */ { 0x56, 0xA8, 0xF5 },
	/* ch 11 */ { 0x56, 0xAA, 0x3D },
	/* ch 12 */ { 0x56, 0xAB, 0x85 },
	/* ch 13 */ { 0x56, 0xAC, 0xCC },
	/* ch 14 */ { 0x56, 0xAE, 0x14 },
	/* ch 15 */ { 0x56, 0xAF, 0x5C },
	/* ch 16 */ { 0x56, 0xB0, 0xA3 },
	/* ch 17 */ { 0x56, 0xB1, 0xEB },
	/* ch 18 */ { 0x56, 0xB3, 0x33 },
	/* ch 19 */ { 0x56, 0xB4, 0x7A },
	/* ch 20 */ { 0x56, 0xB5, 0xC1 },

	/* ch 21 */ { 0x56, 0xE6, 0x65 },
	/* ch 22 */ { 0x56, 0xE7, 0xAC },
	/* ch 23 */ { 0x56, 0xE8, 0xF3 },
	/* ch 24 */ { 0x56, 0xEA, 0x3B },
	/* ch 25 */ { 0x56, 0xEB, 0x83 },
	/* ch 26 */ { 0x56, 0xEC, 0xCA },
	/* ch 27 */ { 0x56, 0xEE, 0x12 },
	/* ch 28 */ { 0x56, 0xEF, 0x5A },
	/* ch 29 */ { 0x56, 0xF0, 0xA1 },
	/* ch 30 */ { 0x56, 0xF1, 0xE9 },
	/* ch 31 */ { 0x56, 0xF3, 0x31 }
};

#ifdef __cplusplus
}
#endif

#endif

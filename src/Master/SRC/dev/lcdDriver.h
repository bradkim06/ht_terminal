#ifndef _LCD_DRIVER_H_
#define _LCD_DRIVER_H_

#include "common_header.h"
// MSP430F412를 사용하여 LCD를 Driving 하기 위한 코드

// 회로 구성
//  1) LCD 장착 여부 확인용 핀 : Port 1_4
//  2) 통신          MSP430F5438(Master)     MSP430F412(Slave)
//          DATA      P10_4 (UART3_TXD)  --> P1_0
//          INT       P1_5               --> P1_1

#define STX 0x02
#define ETX 0x03

#define LEN_LCD_MSG_BODY 11

typedef struct {
	uint8 stx;
	uint8 mtype;
	uint8 body[LEN_LCD_MSG_BODY];
	uint8 checksum;
	uint8 etx;
} LcdMsg_t;

typedef struct {
	uint8 battery : 2, // terminal battery
		lowBatt : 1, // meter battery
		fArrow : 1, rArrow : 1, m3 : 1, notUsed : 1, leak : 1;
} LcdIcon_t;

typedef struct {
	uint8 stx;
	uint8 mtype;
	uint8 digit[9];
	LcdIcon_t icon;
	uint8 flow;
	uint8 checksum;
	uint8 etx;
} LcdMeterMsg_t;

#define MSG_LCD_CLEAR 0x10
#define MSG_LCD_DISPLAY_ALL 0x11
#define MSG_LCD_WAIT 0x20
#define MSG_LCD_DISPLAY_METER 0x30
#define MSG_LCD_TEST 0x40
#define MSG_LCD_DISPLAY_ERR 0x50
#define MSG_LCD_DISPLAY_FW_VER 0x60
#define MSG_LCD_DISPLAY_STRING 0x70

BOOL LCD_Exist();
void LCD_init();
void LCD_enable();
void LCD_disable();
void LCD_clear();
void LCD_displayAll();
void LCD_wait();
void LCD_displayFirmwareVersion();
void LCD_displayCurrentValue();
void LCD_accessModem();
void LCD_notUsed();
void LCD_updateMeterValue(uint8 *unit);
void LCD_redraw();
void LCD_displayString(char *str);
void LCD_displayError(int errCode);
#endif // _LCD_DRIVER_H_

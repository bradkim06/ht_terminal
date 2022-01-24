#include <ctype.h>
#include "common_header.h"
#include "app.h"
#include "osal_Timer.h"
#include "port_desc.h"
#include "check_meter_misc.h"
#include "lcdDriver.h"
#include "uart.h"
#include "meter.h"
#include "battery.h"

#if LCD_THROUGH_GPIO
// INT -  P4.0  - activate(1), deactivate(0)
#define DEACTIVATE_LCD_INTERRUPT()                                                                 \
	do {                                                                                       \
		P4SEL &= ~0x01;                                                                    \
		P4DIR |= 0x01;                                                                     \
		P4OUT &= ~0x01;                                                                    \
	} while (0);
#define ACTIVATE_LCD_INTERRUPT()                                                                   \
	do {                                                                                       \
		P4SEL &= ~0x01;                                                                    \
		P4DIR |= 0x01;                                                                     \
		P4OUT |= 0x01;                                                                     \
	} while (0);
// DATA - P3.1
#define LCD_TX_DATA_LOW()                                                                          \
	do {                                                                                       \
		P3SEL &= ~0x02;                                                                    \
		P3DIR |= 0x02;                                                                     \
		P3OUT &= ~0x02;                                                                    \
	} while (0);
#define LCD_TX_DATA_HIGH()                                                                         \
	do {                                                                                       \
		P3SEL &= ~0x02;                                                                    \
		P3DIR |= 0x02;                                                                     \
		P3OUT |= 0x02;                                                                     \
	} while (0);
#else
#define DEACTIVATE_LCD_INTERRUPT()                                                                 \
	do {                                                                                       \
		P3SEL &= ~0x20;                                                                    \
		P3DIR |= 0x20;                                                                     \
		P3OUT &= ~0x20;                                                                    \
	} while (0);
#define ACTIVATE_LCD_INTERRUPT()                                                                   \
	do {                                                                                       \
		P3SEL &= ~0x20;                                                                    \
		P3DIR |= 0x20;                                                                     \
		P3OUT |= 0x20;                                                                     \
	} while (0);
#endif

#define ICON_NORMAL 0
#define ICON_TEMP 1

struct {
	uint8 bLCD_exist;
	uint8 *txMsg;
	uint8 len;
#if LCD_THROUGH_GPIO
	uchar bytePos;
	uchar bitPos;
	uchar txData;
#endif
} lcdCon;

static struct {
	uint8 value[4];
	int decimalPoint;
	LcdIcon_t iconNormal;
	LcdIcon_t iconTemp;
} currentLCD;

BOOL LCD_Exist()
{
#define LCD_EXIST_CHECK_PIN 4 // P1.4  - LCD가 있는 제품은 GND에 연결되어 있음
	int exist = 0;

	P1DIR &= ~(1 << LCD_EXIST_CHECK_PIN);
	P1REN |= (1 << LCD_EXIST_CHECK_PIN);
	P1OUT |= (1 << LCD_EXIST_CHECK_PIN);

	MISC_delayUs(1);
	if (P1IN & (1 << LCD_EXIST_CHECK_PIN)) {
		exist = 0;
	} else {
		exist = 1;
	}

	P1REN &= ~(1 << LCD_EXIST_CHECK_PIN);
	P1DIR |= (1 << LCD_EXIST_CHECK_PIN);
	P1OUT &= ~(1 << LCD_EXIST_CHECK_PIN);

	return exist;
}

void LCD_init()
{
	memset(&lcdCon, 0, sizeof(lcdCon));
	lcdCon.bLCD_exist = LCD_Exist();

	memset(&currentLCD, 0, sizeof(currentLCD));
	currentLCD.iconNormal.battery = BATT_getLevel();

	DEACTIVATE_LCD_INTERRUPT();
}

void LCD_enable()
{
	DEACTIVATE_LCD_INTERRUPT();
#if LCD_THROUGH_GPIO
	LCD_TX_DATA_HIGH();
#else
	UartInitParam_t param = { .isTxEnable = TRUE,
				  .isRxEnable = FALSE,
				  .baudrate = UART_BAUDRATE_9600,
				  .parity = UART_PARITY_NONE,
				  .stopbit = UART_STOPBIT_ONE,
				  .charLen = UART_8BITS_CHAR };

	UART_open(UART_A0, &param);
#endif
	ACTIVATE_LCD_INTERRUPT();
}

void LCD_disable()
{
#if LCD_THROUGH_GPIO
	LCD_TX_DATA_LOW();
#else
	UART_close(UART_A0, GPIO_LOW);
#endif
	DEACTIVATE_LCD_INTERRUPT();
}

#if LCD_THROUGH_GPIO
void startTimer0(void)
{
	TA0R = 0;
	TA0CCR0 = 3000;
	TA0CTL = TASSEL_2 + MC_1 + TACLR; // SMCLK(8MHz), Up mode, clear TAR
}

void sendNextData()
{
	if (lcdCon.bytePos >= lcdCon.len) {
		TA0CCTL0 &= ~CCIE; // CCR0 interrupt disable
	} else {
		lcdCon.txData = *lcdCon.txMsg++;
		lcdCon.bytePos++;
		lcdCon.bitPos = 11; // start(1) + data(8) + stop(2)
		TA0CCTL0 |= CCIE; // CCR0 interrupt enabled
	}
}

#pragma vector = TIMER0_A0_VECTOR
__interrupt void TIMER0_A0_ISR(void)
{
	TA0R = 0;
	// 760 ~ 855의 범위에서 양호 --> 중간값인 810 적용(이론치는 8,000,000/9600 --> 833임)
	TA0CCR0 = 810;
	TA0CTL = TASSEL_2 + MC_1 + TACLR; // SMCLK(8MHz), Up mode, clear TAR

	if (lcdCon.bitPos >= 11) { // start bit
		LCD_TX_DATA_LOW();
	} else if (lcdCon.bitPos < 3) { // stop bit
		LCD_TX_DATA_HIGH();
	} else {
		if (lcdCon.txData & 0x01) {
			LCD_TX_DATA_HIGH();
		} else {
			LCD_TX_DATA_LOW();
		}
		lcdCon.txData >>= 1;
	}

	if (--lcdCon.bitPos > 0) {
		TA0CCTL0 |= CCIE; // CCR0 interrupt enabled
	} else {
		sendNextData();
	}
}
#endif

static void lcdDataTx()
{
#if LCD_THROUGH_UART
	UART_send(UART_A0, lcdCon.txMsg, lcdCon.len, FALSE);
#else
	MISC_delayUs(400);
	startTimer0();
	sendNextData();
#endif
}

static void sendMsgToLCD(LcdMsg_t *msg)
{
	if (lcdCon.bLCD_exist == 0) {
		return;
	}

	msg->stx = STX;
	msg->etx = ETX;

	uint8 checksum = 0;
	uint8 msgLen = sizeof(LcdMsg_t) - 3; // 3: stx, checksum, etx

	uint8 *p = &msg->mtype;
	for (int i = 0; i < msgLen; i++) {
		checksum ^= *p++;
	}
	msg->checksum = checksum;

	LCD_enable();

	lcdCon.txMsg = (uint8 *)msg;
	lcdCon.len = sizeof(LcdMsg_t);
#if LCD_THROUGH_GPIO
	lcdCon.bytePos = 0;
#endif

	lcdDataTx();

	for (int i = 0; i < lcdCon.len; i++) {
		// 9600bps이므로 1바이트당 1ms가 걸리지 않음
		MISC_delayMs(1);
	}

	LCD_disable();

	MISC_delayMs(50);
}

static void LCD_displayValue(int icon)
{
	LcdMeterMsg_t msg;
	memset(&msg, 0, sizeof(msg));

	msg.mtype = MSG_LCD_DISPLAY_METER;

	for (int i = 0; i < 4; i++) {
		msg.digit[i * 2 + 0] = ((currentLCD.value[i] & 0xf0) >> 4) + 0x30;
		msg.digit[i * 2 + 1] = (currentLCD.value[i] & 0x0f) + 0x30;
	}

	msg.digit[8] = currentLCD.decimalPoint; // 소숫점 아래 자리 수

	if (icon == ICON_NORMAL) {
		memcpy(&msg.icon, &currentLCD.iconNormal, 1);
	} else {
		memcpy(&msg.icon, &currentLCD.iconTemp, 1);
	}
	msg.flow = 0;

	sendMsgToLCD((LcdMsg_t *)&msg);
}

void LCD_clear()
{
	LcdMsg_t msg;
	memset(&msg, 0, sizeof(msg));

	msg.mtype = MSG_LCD_CLEAR;
	sendMsgToLCD(&msg);
}

void LCD_displayAll()
{
	LcdMsg_t msg;
	memset(&msg, 0, sizeof(msg));

	msg.mtype = MSG_LCD_DISPLAY_ALL;
	sendMsgToLCD(&msg);
}

void LCD_wait()
{
	LcdMsg_t msg;
	memset(&msg, 0, sizeof(msg));

	msg.mtype = MSG_LCD_WAIT;
	sendMsgToLCD(&msg);
}

void LCD_displayFirmwareVersion()
{
	LcdMsg_t msg;
	memset(&msg, 0, sizeof(msg));

	msg.mtype = MSG_LCD_DISPLAY_FW_VER;

	char *p = (char *)msg.body;
	snprintf(p, LEN_LCD_MSG_BODY, "%s", FIRMWARE_VER);
	sendMsgToLCD(&msg);
}

void LCD_accessModem()
{
	memcpy(&currentLCD.iconTemp, &currentLCD.iconNormal, 1);
	currentLCD.iconTemp.fArrow = 1;
	currentLCD.iconTemp.rArrow = 1;
	LCD_displayValue(ICON_TEMP);
}

void LCD_notUsed()
{
	currentLCD.iconNormal.notUsed = 1;
	LCD_displayValue(ICON_NORMAL);
}

void LCD_redraw()
{
	LCD_displayValue(ICON_NORMAL);
}

void LCD_updateMeterValue(uint8 *unit)
{
	MeterUnitData_t *p = (MeterUnitData_t *)unit;

	memcpy(currentLCD.value, p->meterData, 4);
	memcpy(&currentLCD.iconNormal, &p->icon, 1);
	currentLCD.decimalPoint = METER_getMeterCaliber_dp() & 0x0f;
	currentLCD.iconNormal.battery = BATT_getLevel();

	LCD_displayValue(ICON_NORMAL);
}

void LCD_displayString(char *str)
{
	LcdMsg_t msg;
	memset(&msg, 0, sizeof(msg));

	msg.mtype = MSG_LCD_DISPLAY_STRING;

	int len = strlen(str);
	if (len > 11) {
		len = 11;
	}

	memcpy(msg.body, str, len);
	sendMsgToLCD(&msg);
}

void LCD_displayError(int errCode)
{
	LcdMsg_t msg;
	memset(&msg, 0, sizeof(msg));

	msg.mtype = MSG_LCD_DISPLAY_ERR;

	msg.body[0] = (uint8)errCode;

	sendMsgToLCD(&msg);
}

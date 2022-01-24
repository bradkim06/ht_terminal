#include <msp430.h>
#include <stdarg.h> // va_list, va_start, va_end
#include "common_header.h"
#include "port_desc.h"
#include "uart.h"
#include "check_meter_misc.h"
#include "osal_Timer.h"
#include "app.h"
#include "meter.h"
#include "lcdDriver.h"
#include "shell.h"
#include "rtcAlarm.h"
#include "modem.h"
#include "test.h"

#define USCI_A0_TX_PIN BM(4)
#define USCI_A0_RX_PIN BM(5)
#define USCI_A1_TX_PIN BM(6)
#define USCI_A1_RX_PIN BM(7)
#define USCI_A2_TX_PIN BM(4)
#define USCI_A2_RX_PIN BM(5)
#define USCI_A3_TX_PIN BM(4)
#define USCI_A3_RX_PIN BM(5)

#define USCI_A0_PIN (USCI_A0_TX_PIN | USCI_A0_RX_PIN)
#define USCI_A1_PIN (USCI_A1_TX_PIN | USCI_A1_RX_PIN)
#define USCI_A2_PIN (USCI_A2_TX_PIN | USCI_A2_RX_PIN)
#define USCI_A3_PIN (USCI_A3_TX_PIN | USCI_A3_RX_PIN)

#define USCI_A0_UART_TX_ENABLE()                                                                   \
	do {                                                                                       \
		P3SEL |= USCI_A0_TX_PIN;                                                           \
	} while (0);
#define USCI_A0_UART_RX_ENABLE()                                                                   \
	do {                                                                                       \
		P3SEL |= USCI_A0_RX_PIN;                                                           \
	} while (0);
#define USCI_A1_UART_TX_ENABLE()                                                                   \
	do {                                                                                       \
		P5SEL |= USCI_A1_TX_PIN;                                                           \
	} while (0);
#define USCI_A1_UART_RX_ENABLE()                                                                   \
	do {                                                                                       \
		P5SEL |= USCI_A1_RX_PIN;                                                           \
	} while (0);
#define USCI_A2_UART_TX_ENABLE()                                                                   \
	do {                                                                                       \
		P9SEL |= USCI_A2_TX_PIN;                                                           \
	} while (0);
#define USCI_A2_UART_RX_ENABLE()                                                                   \
	do {                                                                                       \
		P9SEL |= USCI_A2_RX_PIN;                                                           \
	} while (0);
#define USCI_A3_UART_TX_ENABLE()                                                                   \
	do {                                                                                       \
		P10SEL |= USCI_A3_TX_PIN;                                                          \
	} while (0);
#define USCI_A3_UART_RX_ENABLE()                                                                   \
	do {                                                                                       \
		P10SEL |= USCI_A3_RX_PIN;                                                          \
	} while (0);

#define USCI_A0_GPIO_LOW()                                                                         \
	do {                                                                                       \
		P3SEL &= ~USCI_A0_PIN;                                                             \
		P3DIR |= USCI_A0_PIN;                                                              \
		P3OUT &= ~USCI_A0_PIN;                                                             \
	} while (0);
#define USCI_A0_GPIO_INPUT()                                                                       \
	do {                                                                                       \
		P3SEL &= ~USCI_A0_PIN;                                                             \
		P3DIR &= ~USCI_A0_PIN;                                                             \
		P3REN &= ~USCI_A0_PIN;                                                             \
	} while (0);
#define USCI_A1_GPIO_LOW()                                                                         \
	do {                                                                                       \
		P5SEL &= ~USCI_A1_PIN;                                                             \
		P5DIR |= USCI_A1_PIN;                                                              \
		P5OUT &= ~USCI_A1_PIN;                                                             \
	} while (0);
#define USCI_A1_GPIO_INPUT()                                                                       \
	do {                                                                                       \
		P5SEL &= ~USCI_A1_PIN;                                                             \
		P5DIR &= ~USCI_A1_PIN;                                                             \
		P5REN &= ~USCI_A1_PIN;                                                             \
	} while (0);
#define USCI_A2_GPIO_LOW()                                                                         \
	do {                                                                                       \
		P9SEL &= ~USCI_A2_PIN;                                                             \
		P9DIR |= USCI_A2_PIN;                                                              \
		P9OUT &= ~USCI_A2_PIN;                                                             \
	} while (0);
#define USCI_A2_GPIO_INPUT()                                                                       \
	do {                                                                                       \
		P9SEL &= ~USCI_A2_PIN;                                                             \
		P9DIR &= ~USCI_A2_PIN;                                                             \
		P9REN &= ~USCI_A2_PIN;                                                             \
	} while (0);
#define USCI_A3_GPIO_LOW()                                                                         \
	do {                                                                                       \
		P10SEL &= ~USCI_A3_PIN;                                                            \
		P10DIR |= USCI_A3_PIN;                                                             \
		P10OUT &= ~USCI_A3_PIN;                                                            \
	} while (0);
#define USCI_A3_GPIO_INPUT()                                                                       \
	do {                                                                                       \
		P10SEL &= ~USCI_A3_PIN;                                                            \
		P10DIR &= ~USCI_A3_PIN;                                                            \
		P10REN &= ~USCI_A3_PIN;                                                            \
	} while (0);

typedef struct {
	int len;
	int wpos;
	int rpos;
	char *buf;
} UartBuf_t;

typedef struct {
	UartBuf_t *txBuf;
	UartBuf_t *rxBuf;
} UartIoBuf_t;

static UartIoBuf_t *uartA0 = NULL;
static UartIoBuf_t *uartA1 = NULL;
static UartIoBuf_t *uartA2 = NULL;
static UartIoBuf_t *uartA3 = NULL;

static int IsPrintOn = 1;

static void *mallocBuf(int bufSize)
{
	void *p = (void *)malloc(bufSize);
	if (p == NULL) {
		printf("malloc failed\n");
		MISC_delayMs(1000);
		REBOOT_SYSTEM();
	}
	return p;
}

static void mallocUartBuf(UartIoBuf_t **ioBuf, int txBufLen, int rxBufLen)
{
	if ((*ioBuf) != NULL) {
		if ((*ioBuf)->txBuf->buf != NULL) {
			free((*ioBuf)->txBuf->buf);
		}
		if ((*ioBuf)->txBuf != NULL) {
			free((*ioBuf)->txBuf);
		}
		if ((*ioBuf)->rxBuf->buf != NULL) {
			free((*ioBuf)->rxBuf->buf);
		}
		if ((*ioBuf)->rxBuf != NULL) {
			free((*ioBuf)->rxBuf);
		}
		free(*ioBuf);
	}

	(*ioBuf) = (UartIoBuf_t *)mallocBuf(sizeof(UartIoBuf_t));
	if (txBufLen > 0) {
		(*ioBuf)->txBuf = (UartBuf_t *)mallocBuf(sizeof(UartBuf_t));
		(*ioBuf)->txBuf->buf = (char *)mallocBuf(txBufLen);
		(*ioBuf)->txBuf->len = txBufLen;
		(*ioBuf)->txBuf->wpos = (*ioBuf)->txBuf->rpos = 0;
		memset((*ioBuf)->txBuf->buf, 0, txBufLen);
	} else {
		(*ioBuf)->txBuf = NULL;
	}

	if (rxBufLen > 0) {
		(*ioBuf)->rxBuf = (UartBuf_t *)mallocBuf(sizeof(UartBuf_t));
		(*ioBuf)->rxBuf->buf = (char *)mallocBuf(rxBufLen);
		(*ioBuf)->rxBuf->len = rxBufLen;
		(*ioBuf)->rxBuf->wpos = (*ioBuf)->rxBuf->rpos = 0;
		memset((*ioBuf)->rxBuf->buf, 0, rxBufLen);
	} else {
		(*ioBuf)->rxBuf = NULL;
	}
}

void UART_init()
{
	mallocUartBuf(&uartA0, LCD_TX_BUF_LEN, 0);
	mallocUartBuf(&uartA1, DEBUG_TX_BUF_LEN, DEBUG_RX_BUF_LEN);
	mallocUartBuf(&uartA2, MODEM_TX_BUF_LEN, MODEM_RX_BUF_LEN);
	mallocUartBuf(&uartA3, METER_TX_BUF_LEN, METER_RX_BUF_LEN);
}

void UART_debugMode()
{
	mallocUartBuf(&uartA0, LCD_TX_BUF_LEN, 0);
	mallocUartBuf(&uartA1, METER_TX_BUF_LEN, METER_RX_BUF_LEN);
	mallocUartBuf(&uartA2, MODEM_TX_BUF_LEN, MODEM_RX_BUF_LEN);
	mallocUartBuf(&uartA3, DEBUG_TX_BUF_LEN, DEBUG_RX_BUF_LEN);

	UartInitParam_t param = { .isTxEnable = TRUE,
				  .isRxEnable = TRUE,
				  .baudrate = UART_BAUDRATE_115200,
				  .parity = UART_PARITY_NONE,
				  .stopbit = UART_STOPBIT_ONE,
				  .charLen = UART_8BITS_CHAR };

	UART_open(UART_A3, &param);
}

void UART_open(UartNum_t num, UartInitParam_t *param)
{
	static volatile unsigned char *UART_BASE = NULL;

	// Set module base address and pin function
	if (num == UART_A0) {
		UART_BASE = ((unsigned char *)USCI_A0_BASE);
		if (param->isTxEnable) {
			USCI_A0_UART_TX_ENABLE();
		}
		if (param->isRxEnable) {
			USCI_A0_UART_RX_ENABLE();
		}
	} else if (num == UART_A1) {
		UART_BASE = ((unsigned char *)USCI_A1_BASE);
		if (param->isTxEnable) {
			USCI_A1_UART_TX_ENABLE();
		}
		if (param->isRxEnable) {
			USCI_A1_UART_RX_ENABLE();
		}
	} else if (num == UART_A2) {
		UART_BASE = ((unsigned char *)USCI_A2_BASE);
		if (param->isTxEnable) {
			USCI_A2_UART_TX_ENABLE();
		}
		if (param->isRxEnable) {
			USCI_A2_UART_RX_ENABLE();
		}
	} else if (num == UART_A3) {
		UART_BASE = ((unsigned char *)USCI_A3_BASE);
		if (param->isTxEnable) {
			USCI_A3_UART_TX_ENABLE();
		}
		if (param->isRxEnable) {
			USCI_A3_UART_RX_ENABLE();
		}
	} else {
		return;
	}

	// software reset enabled
	*(UART_BASE + 0x00) |= UCSWRST;

	// Set parity
	if (param->parity == UART_PARITY_NONE) {
		*(UART_BASE + 0x01) &= ~(UCPEN);
	} else if (param->parity == UART_PARITY_ODD) {
		*(UART_BASE + 0x01) |= UCPEN;
		*(UART_BASE + 0x01) &= ~(UCPAR);
	} else if (param->parity == UART_PARITY_ODD) {
		*(UART_BASE + 0x01) |= UCPEN;
		*(UART_BASE + 0x01) |= UCPAR;
	} else {
		return;
	}

	// Set stop bits
	if (param->stopbit == UART_STOPBIT_ONE) {
		*(UART_BASE + 0x01) &= ~(UCSPB);
	} else if (param->stopbit == UART_STOPBIT_TWO) {
		*(UART_BASE + 0x01) |= UCSPB;
	} else {
		return;
	}

	// Set character length
	if (param->charLen == UART_8BITS_CHAR) {
		*(UART_BASE + 0x01) &= ~(UC7BIT);
	} else if (param->charLen == UART_7BITS_CHAR) {
		*(UART_BASE + 0x01) |= UC7BIT;
	} else {
		return;
	}

	// Set baudrate with clock source
	if (param->baudrate == UART_BAUDRATE_600) {
		*(UART_BASE + 0x00) |= UCSSEL_1; // use ACLK
		*(UART_BASE + 0x06) = 0x35;
		*(UART_BASE + 0x07) = 0x00;
		*(UART_BASE + 0x08) |= UCBRS_7 + UCBRF_0;
	} else if (param->baudrate == UART_BAUDRATE_1200) {
		*(UART_BASE + 0x00) |= UCSSEL_1; // use ACLK
		*(UART_BASE + 0x06) = 0x1b;
		*(UART_BASE + 0x07) = 0x00;
		*(UART_BASE + 0x08) |= UCBRS_2 + UCBRF_0;
	} else if (param->baudrate == UART_BAUDRATE_2400) {
		*(UART_BASE + 0x00) |= UCSSEL_1; // use ACLK
		*(UART_BASE + 0x06) = 0x0D;
		*(UART_BASE + 0x07) = 0x00;
		*(UART_BASE + 0x08) |= UCBRS_6 + UCBRF_0;
	} else if (param->baudrate == UART_BAUDRATE_9600) {
		*(UART_BASE + 0x00) |= UCSSEL_2; // use SMCLK
		*(UART_BASE + 0x06) = 0x46; // 0x41
		*(UART_BASE + 0x07) = 0x03;
		*(UART_BASE + 0x08) |= UCBRS_2 + UCBRF_0;
	} else if (param->baudrate == UART_BAUDRATE_38400) {
		*(UART_BASE + 0x00) |= UCSSEL_2; // use SMCLK
		*(UART_BASE + 0x06) = 0xDC;
		*(UART_BASE + 0x07) = 0;
		*(UART_BASE + 0x08) |= UCBRS_3 + UCBRF_0;
	} else if (param->baudrate == UART_BAUDRATE_115200) {
		*(UART_BASE + 0x00) |= UCSSEL_2; // use SMCLK
		*(UART_BASE + 0x06) = 0x45;
		*(UART_BASE + 0x07) = 0;
		*(UART_BASE + 0x08) |= UCBRS_4 + UCBRF_0;
	} else {
		return;
	}

	// Set LSB first
	*(UART_BASE + 0x01) &= ~(UCMSB);

	// Set Uart mode with asynchronous mode
	*(UART_BASE + 0x01) &= ~(UCMODE0 | UCMODE1);

	// software reset disabled
	*(UART_BASE + 0x00) &= ~UCSWRST;

	// disable RX/TX interrupt
	*(UART_BASE + 0x1C) &= ~(UCTXIE | UCRXIE);
	// Enable RX interrupt if Rx is enabled
	if (param->isRxEnable) {
		*(UART_BASE + 0x1C) |= UCRXIE;
	}

	*(UART_BASE + 0x1D) |= UCTXIFG; // TX buffer interrupt clear
	*(UART_BASE + 0x1D) &= ~UCRXIFG; // RX buffer interrupt clear
}

void UART_close(UartNum_t num, UartPinMode_t mode)
{
	static volatile unsigned char *UART_BASE = NULL;

	// Set module base address and pin function
	if (num == UART_A0) {
		UART_BASE = ((unsigned char *)USCI_A0_BASE);
		if (mode == GPIO_LOW) {
			USCI_A0_GPIO_LOW();
		} else {
			USCI_A0_GPIO_INPUT();
		}
	} else if (num == UART_A1) {
		UART_BASE = ((unsigned char *)USCI_A1_BASE);
		if (mode == GPIO_LOW) {
			USCI_A1_GPIO_LOW();
		} else {
			USCI_A1_GPIO_INPUT();
		}
	} else if (num == UART_A2) {
		UART_BASE = ((unsigned char *)USCI_A2_BASE);
		if (mode == GPIO_LOW) {
			USCI_A2_GPIO_LOW();
		} else {
			USCI_A2_GPIO_INPUT();
		}
	} else if (num == UART_A3) {
		UART_BASE = ((unsigned char *)USCI_A3_BASE);
		if (mode == GPIO_LOW) {
			USCI_A3_GPIO_LOW();
		} else {
			USCI_A3_GPIO_INPUT();
		}
	} else {
		return;
	}

	*(UART_BASE + 0x1C) &= ~(UCTXIE | UCRXIE); // disable RX/TX interrupt
	*(UART_BASE + 0x1D) |= UCTXIFG; // TX buffer interrupt clear
	*(UART_BASE + 0x1D) &= ~UCRXIFG; // RX buffer interrupt clear
}

int printf(const char *format, ...)
{
#define BUF_SIZE DEBUG_TX_BUF_LEN

	if (IsPrintOn) {
		int len = 0;
		char buf[BUF_SIZE];

		va_list arg;
		va_start(arg, format);
		// Console 출력 시 New line을 위한 1byte 고려.
		len += vsnprintf(buf + len, BUF_SIZE - 1, format, arg);
		va_end(arg);

		if (buf[len - 1] == '\n') {
			buf[len++] = '\r';
		}

		if (conf.debugPrint == 2) {
			UART_send(UART_A3, buf, len, FALSE);
		} else {
			UART_send(UART_A1, buf, len, FALSE);
		}
	}

	return 0;
}

int printf_ts(const char *format, ...)
{
#define BUF_SIZE DEBUG_TX_BUF_LEN

	if (IsPrintOn) {
		int len = 0;
		char buf[BUF_SIZE];
		Date_t date;

		RTC_read(&date);
		snprintf(buf, 30, "[%04d-%02d-%02d %02d:%02d:%02d]", date.year, date.mon, date.day,
			 date.hour, date.min, date.sec);
		len = strlen(buf);

		va_list arg;
		va_start(arg, format);
		// Console 출력 시 New line을 위한 1byte 고려.
		len += vsnprintf(buf + len, BUF_SIZE - 1, format, arg);
		va_end(arg);

		if (buf[len - 1] == '\n') {
			buf[len++] = '\r';
		}

		if (conf.debugPrint == 2) {
			UART_send(UART_A3, buf, len, FALSE);
		} else {
			UART_send(UART_A1, buf, len, FALSE);
		}
	}

	return 0;
}

void PRINT_enable()
{
	if (IsPrintOn == 0) {
		return;
	}

	UartInitParam_t param = { .isTxEnable = TRUE,
				  .isRxEnable = TRUE,
				  .baudrate = UART_BAUDRATE_115200,
				  .parity = UART_PARITY_NONE,
				  .stopbit = UART_STOPBIT_ONE,
				  .charLen = UART_8BITS_CHAR };

	UART_open(UART_A1, &param);
}

void PRINT_disable()
{
	UART_close(UART_A1, GPIO_LOW);
}

void PRINT_stop()
{
	IsPrintOn = 0;
}

void PRINT_resume()
{
	if (TEST_isTestMode()) {
		IsPrintOn = 1;
	} else {
		IsPrintOn = conf.debugPrint;
	}
}

void PRINT_string(char *p, int len)
{
	printf_ts("");
	for (int i = 0; i < len; i++) {
		char ch = *(p + i);
#if LORA_DEVICE
		if (ch != '\r') {
			printf("%c", ch);
		}
#else
		if (ch == '\r' || ch == '\n') {
			if (TEST_isTestMode())
				printf("%c", ch);
			else
				printf(".");
		} else {
			printf("%c", ch);
		}
#endif
	}
	printf("\n");
}

void PRINT_hexBuffer(uint8 *p, int len)
{
#define DUMP_OFFSET "        00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F"
#define DUMP_LINE "-------------------------------------------------------"
#define DUMP_ADDR_FORM "0x%04X: "
#define DUMP_VALUE_FORM "%02X "

	printf("[HEX data len:%d]\n", len);

	unsigned char *bp = (unsigned char *)(p);
	printf(DUMP_OFFSET "\n");
	printf(DUMP_LINE);
	for (uint16 i = 0; i < len; i++) {
		if (0 == i % 16)
			printf("\r\n" DUMP_ADDR_FORM, i);
		printf(DUMP_VALUE_FORM, bp[i]);
	}
	printf("\r\n" DUMP_LINE "\n");
}

void UART_send(UartNum_t num, void *buffer, int len, BOOL isAppendCR)
{
	static volatile unsigned char *UART_BASE = NULL;
	UartBuf_t *p = NULL;
	if (num == UART_A0) {
		UART_BASE = ((unsigned char *)USCI_A0_BASE);
		p = uartA0->txBuf;
	} else if (num == UART_A1) {
		UART_BASE = ((unsigned char *)USCI_A1_BASE);
		p = uartA1->txBuf;
	} else if (num == UART_A2) {
		UART_BASE = ((unsigned char *)USCI_A2_BASE);
		p = uartA2->txBuf;
	} else if (num == UART_A3) {
		UART_BASE = ((unsigned char *)USCI_A3_BASE);
		p = uartA3->txBuf;
	}

	if (p == NULL) {
		return;
	}

	halIntState_t intState;
	for (int index = 0; index < len; index++) {
		if (((p->wpos + 1) % p->len) == p->rpos) { // buffer full
			MISC_delayMs(1);
		}

		HAL_ENTER_CRITICAL_SECTION(intState);
		p->buf[p->wpos++] = *((uint8 *)buffer + index);
		if (p->wpos >= p->len) {
			p->wpos = 0;
		}
		HAL_EXIT_CRITICAL_SECTION(intState);
	}

	if (isAppendCR) {
		HAL_ENTER_CRITICAL_SECTION(intState);
		p->buf[p->wpos++] = '\r';
		if (p->wpos >= p->len) {
			p->wpos = 0;
		}
		HAL_EXIT_CRITICAL_SECTION(intState);
	}

	*(UART_BASE + 0x1C) |= UCTXIE; // Enable TX interrupt
}

int UART_receive(UartNum_t num, void *buffer, int limit)
{
	UartBuf_t *p = NULL;
	if (num == UART_A0) {
		p = uartA0->rxBuf;
	} else if (num == UART_A1) {
		p = uartA1->rxBuf;
	} else if (num == UART_A2) {
		p = uartA2->rxBuf;
	} else if (num == UART_A3) {
		p = uartA3->rxBuf;
	}

	if (p == NULL) {
		return 0;
	}

	int len = 0;
	halIntState_t intState;
	while (p->wpos != p->rpos) {
		HAL_ENTER_CRITICAL_SECTION(intState);
		*((uint8 *)buffer + len) = p->buf[p->rpos];
		if (++p->rpos >= p->len) {
			p->rpos = 0;
		}
		HAL_EXIT_CRITICAL_SECTION(intState);
		if (++len >= limit) {
			break;
		}
	}

	return len;
}

BOOL UART_getChar(UartNum_t num, char *ch)
{
	UartBuf_t *p = NULL;
	if (num == UART_A0) {
		p = uartA0->rxBuf;
	} else if (num == UART_A1) {
		p = uartA1->rxBuf;
	} else if (num == UART_A2) {
		p = uartA2->rxBuf;
	} else if (num == UART_A3) {
		p = uartA3->rxBuf;
	}

	if (p == NULL) {
		return 0;
	}

	if (p->wpos == p->rpos) {
		return FALSE;
	}

	*ch = p->buf[p->rpos];
	halIntState_t intState;
	HAL_ENTER_CRITICAL_SECTION(intState);
	if (++p->rpos >= p->len) {
		p->rpos = 0;
	}
	HAL_EXIT_CRITICAL_SECTION(intState);

	return TRUE;
}

void set_meterPortForBSL()
{
	UCA3IE &= ~(UCTXIE | UCRXIE); // disable RX/TX interrupt
	UCA3IFG |= UCTXIFG; // TX buffer interrupt clear
	UCA3IFG &= ~(UCRXIFG); // RX buffer interrupt clear

	P1SEL &= ~0x06; // P1.1, P1.2 GPIO
	P1DIR &= ~0x06; // P1.1, P1.2 as input
	P1OUT &= ~0x06; // P1.1, P1.2 Low
	P1REN &= ~0x06; // no pull up/down

	P10SEL &= ~0x30; // P10.4, P10.5 GPIO
	P10DIR &= ~0x30; // P10.4, P10.5 input
	P10OUT &= ~0x30; // P10.4, P10.5 low
	P10REN &= ~0x30; // no pull up/down
}

#if LCD_THROUGH_UART
#pragma vector = USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void)
{
	if (UCA0IFG & UCTXIFG) {
		if (uartA0->txBuf == NULL) {
			UCA0IE &= ~UCTXIE;
			return;
		}

		UartBuf_t *p = uartA0->txBuf;
		if (p->rpos == p->wpos) {
			UCA0IE &= ~UCTXIE; // 더 이상 보낼 데이터가 없으면 tx interrupt를 disable
		} else {
			UCA0TXBUF = p->buf[p->rpos++]; // send the data
			if (p->rpos >= p->len) {
				p->rpos = 0;
			}
		}
	}

	// rx interrupt를 사용하지 않음 - 혹시 발생하면 즉시 disable
	if (UCA0IFG & UCRXIFG) {
		UCA0IFG &= ~UCRXIFG;
		if (uartA0->rxBuf == NULL) {
			UCA0IE &= ~UCRXIE;
			return;
		}

		UartBuf_t *p = uartA0->rxBuf;
		if (((p->wpos + 1) % p->len) == p->rpos) {
			p->wpos = p->rpos = 0;
		} else {
			p->buf[p->wpos] = UCA0RXBUF;
			if (++p->wpos >= p->len) {
				p->wpos = 0;
			}
		}
	}
}
#endif

// UART_1 interrupt - 콘솔
#pragma vector = USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void)
{
	if (UCA1IFG & UCTXIFG) { // USCI_A1 TX
		if (uartA1->txBuf == NULL) {
			UCA1IE &= ~UCTXIE;
			return;
		}

		UartBuf_t *p = uartA1->txBuf;
		if (p->rpos == p->wpos) {
			UCA1IE &= ~UCTXIE; // txbuf empty --> disable tx interrupt
		} else {
			UCA1TXBUF = p->buf[p->rpos++]; // send the data
			if (p->rpos >= p->len) {
				p->rpos = 0;
			}
		}
	}

	if (UCA1IFG & UCRXIFG) { // USCI_A1 RX
		UCA1IFG &= ~(UCRXIFG);
		if (uartA1->rxBuf == NULL) {
			UCA1IE &= ~UCRXIE;
			return;
		}

		UartBuf_t *p = uartA1->rxBuf;
		if (((p->wpos + 1) % p->len) == p->rpos) {
			p->wpos = p->rpos = 0;
		} else {
			p->buf[p->wpos] = UCA1RXBUF;
			if (++p->wpos >= p->len) {
				p->wpos = 0;
			}
		}
	}
}

// UART_2 interrupt - SMT/SKT/NB-IoT Modem
#pragma vector = USCI_A2_VECTOR
__interrupt void USCI_A2_ISR(void)
{
	if (UCA2IFG & UCTXIFG) { // USCI_A2 TX
		if (uartA2->txBuf == NULL) {
			UCA2IE &= ~UCTXIE;
			return;
		}

		UartBuf_t *p = uartA2->txBuf;
		if (p->rpos == p->wpos) {
			UCA2IE &= ~UCTXIE; // txbuf empty --> disable tx interrupt
		} else {
			UCA2TXBUF = p->buf[p->rpos++]; // send the data
			if (p->rpos >= p->len) {
				p->rpos = 0;
			}
		}
	}

	if (UCA2IFG & UCRXIFG) { // USCI_A2 RX
		UCA2IFG &= ~(UCRXIFG);
		if (uartA2->rxBuf == NULL) {
			UCA2IE &= ~UCRXIE;
			return;
		}

		UartBuf_t *p = uartA2->rxBuf;
		if (((p->wpos + 1) % p->len) == p->rpos) {
			p->wpos = p->rpos = 0;
		} else {
			p->buf[p->wpos] = UCA2RXBUF;
			if (++p->wpos >= p->len) {
				p->wpos = 0;
			}
			OSAL_startEventTimer(AppTaskId, APP_EVENT_MODEM_RX, (uint32)200);
		}
	}
}

// UART_3 interrupt - Rx는 계량기 데이터 수신용, Tx는 신한 이외의 계량기에 명령 전송용으로 사용
#if !defined(AUX_REPEATER)
#pragma vector = USCI_A3_VECTOR
__interrupt void USCI_A3_ISR(void)
{
	if (UCA3IFG & UCTXIFG) {
		if (uartA3->txBuf == NULL) {
			UCA3IE &= ~UCTXIE;
			return;
		}

		UartBuf_t *p = uartA3->txBuf;
		if (p->rpos == p->wpos) {
			UCA3IE &= ~UCTXIE; // txbuf empty --> disable tx interrupt
		} else {
			UCA3TXBUF = p->buf[p->rpos++]; // send the data
			if (p->rpos >= p->len) {
				p->rpos = 0;
			}
		}
	}

	if (UCA3IFG & UCRXIFG) { // USCI_A3 RX
		UCA3IFG &= ~(UCRXIFG);
		if (uartA3->rxBuf == NULL) {
			UCA3IE &= ~UCRXIE;
			return;
		}

		UartBuf_t *p = uartA3->rxBuf;
		if (((p->wpos + 1) % p->len) == p->rpos) {
			p->wpos = p->rpos = 0;
		} else {
			p->buf[p->wpos] = UCA3RXBUF;
			if (++p->wpos >= p->len) {
				p->wpos = 0;
			}
			OSAL_startEventTimer(AppTaskId, APP_EVENT_METER_RX, (uint32)50);
		}
	}
}
#endif // #if !defined(AUX_REPEATER)

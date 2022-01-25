/**
 * @file uart.h
 * @author ytkim (ytkim@hitecepc.com)
 * @brief
 *  [UART 0]
 *   - TxD (P3.4) : 5438(5419)에서 413으로 LCD 제어 메시지를 전달
 *   - RxD (P3.5) : GPIO mode(5438(5419)이 UART0 TxD pin으로 메시지를 곧 보낼 거라는 점을 413에 알림
 *   - txBuf만 필요
 *  [UART 1]
 *   - TxD (P5.6) : console TX
 *   - RxD (P5.7) : console RX
 *   - txBuf, rxBuf 모두 필요
 *  [UART 2]
 *   - TxD (P9.4) : TxD to SMT RF Module
 *   - RxD (P9.5) : RxD from SMT RF Module
 *   - txBuf 만 필요(rx의 경우 shell에 정의된 commandBuffer를 직접 이용
 *  [UART 3]
 *   - TxD (P10.4) : 신한/MNS 계량기의 경우 GPIO, 다른 계량기는 UART Mode
 *   - RxD (P10.5) : RxD from meter
 *   - 신한/MNS의 경우 rxBuf만 필요, 다른 계량기는 TxBuf/RxBuf 모두 필요
 *
 * @version 0.1
 * @date 2020-04-02
 *
 * @copyright Copyright (c) 2020
 *
 */

#ifndef __UART_H__
#define __UART_H__

#include "common_header.h"
#include "device.h"

#define LCD_TX_BUF_LEN 0x40
#define DEBUG_TX_BUF_LEN 0x200
#define DEBUG_RX_BUF_LEN 0x200
#define MODEM_TX_BUF_LEN 0x200
#define MODEM_RX_BUF_LEN 0x500
#define METER_TX_BUF_LEN 0x40
#define METER_RX_BUF_LEN 0x60

typedef enum {
	GPIO_INPUT = 0,
	GPIO_LOW = 1,
} UartPinMode_t;

typedef enum { UART_A0 = 0, UART_A1 = 1, UART_A2 = 2, UART_A3 = 3 } UartNum_t;

typedef enum {
	UART_BAUDRATE_600 = 0,
	UART_BAUDRATE_1200 = 1,
	UART_BAUDRATE_2400 = 2,
	// UART_BAUDRATE_4800   = 3,
	UART_BAUDRATE_9600 = 4,
	// UART_BAUDRATE_14400  = 5,
	// UART_BAUDRATE_19200  = 6,
	UART_BAUDRATE_38400 = 7,
	// UART_BAUDRATE_57600  = 8,
	UART_BAUDRATE_115200 = 9
} UartBaudrate_t;

typedef enum { UART_PARITY_NONE = 0, UART_PARITY_ODD = 1, UART_PARITY_EVEN = 2 } UartParity_t;

typedef enum { UART_STOPBIT_ONE = 0, UART_STOPBIT_TWO = 1 } UartStopbit_t;

typedef enum { UART_8BITS_CHAR = 0, UART_7BITS_CHAR = 1 } UartCharLen_t;

typedef struct {
	BOOL isTxEnable;
	BOOL isRxEnable;
	UartBaudrate_t baudrate;
	UartParity_t parity;
	UartStopbit_t stopbit;
	UartCharLen_t charLen;
} UartInitParam_t;

// UART library
void UART_init();
void UART_debugMode();
void UART_open(UartNum_t num, UartInitParam_t *param);
void UART_close(UartNum_t num, UartPinMode_t mode);
void UART_send(UartNum_t num, void *buffer, int len, BOOL isAppendCR);
int UART_receive(UartNum_t num, void *buffer, int limit);
BOOL UART_getChar(UartNum_t num, char *ch);

// Default print functions
int printf(const char *format, ...);
int printf_ts(const char *format, ...);

// Additional function print functions
void PRINT_enable();
void PRINT_disable();
void PRINT_stop();
void PRINT_resume();
void PRINT_hexBuffer(uint8 *p, int len);
void PRINT_string(char *p, int len);
void set_meterPortForBSL();
#endif

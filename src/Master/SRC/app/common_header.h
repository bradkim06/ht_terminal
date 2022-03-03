#ifndef _COMMON_HEADER_
#define _COMMON_HEADER_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "device.h"

#ifndef TDD_TEST

#include <msp430.h>

#endif

// Error Code
#define ERROR_CODE_NONE 0 // 정상
#define ERROR_CODE_MODEM 1 // 모뎀 응답 없음
#if NBIOT_DEVICE
#define ERROR_CODE_SIM 2 // SIM 오류
#else // LORA_DEVICE
#define ERROR_CODE_NOT_JOINED 3 // Join 실패
#endif
#define ERROR_CODE_COMM 4 // 통신 실패
#define ERROR_CODE_GET_TIME 5 // 시간 읽기 오류
#define ERROR_CODE_FAIL_ATTACH 6 // 망 접속 실패
#define ERROR_CODE_FAIL_CERTIFY 7 // 플랫폼 인증 실패
#define ERROR_CODE_UNKNOWN 99 // 알 수 없는 오류

// BSL Type
#define TERM_BSL_DISABLE 0
#define TERM_BSL_ENABLE 1

// 단말기 타입
#define TERM_MODEL_UNKNOWN 0
#define TERM_MODEL_HAT_114W 1 // 표시형 단말기
#define TERM_MODEL_HAT_124W 2 // 비표시형 단말기
#define TERM_MODEL_HTM_115W 3 // 스마트 워터미터
#define TERM_MODEL_HAT_314W 4 // 외부표시기
#define TERM_MODEL_HAT_435W 5 // 보조중계기

#define TERM_MODEL_STRING_LEN 8
#define TERM_MODEL_STRING(x)                                                                       \
	((x) == TERM_MODEL_HAT_114W ? "HAT-114W" :                                                 \
	 (x) == TERM_MODEL_HAT_124W ? "HAT-124W" :                                                 \
	 (x) == TERM_MODEL_HTM_115W ? "HTM-115W" :                                                 \
	 (x) == TERM_MODEL_HAT_314W ? "HAT-314W" :                                                 \
	 (x) == TERM_MODEL_HAT_435W ? "HAT-435W" :                                                 \
					    "UNKNOWN")

#define TERM_HAS_LCD(x) ((x) == TERM_MODEL_HAT_114W)
#define SMART_WATER_METER(x) ((x) == TERM_MODEL_HTM_115W)

// Terminal Status
#define DEVICE_STATUS_IDLE 0
#define DEVICE_STATUS_CHECK_MODEM 1
#if NBIOT_DEVICE
#define DEVICE_STATUS_CHECK_SIM 2
#else // LORA_DEVICE
#define DEVICE_STATUS_WAIT_JOIN 3
#endif
#define DEVICE_STATUS_COMM 4
#define DEVICE_STATUS_GET_TIME 5
#define DEVICE_STATUS_UNKNOWN 99

// Network access state
#define ACCESS_STATE_IDLE 0 // 접속 절차를 진행하고 있지 않음 - 새 절차 개시 가능
#define ACCESS_STATE_IN_PROGRESS 1 // 접속 절차 진행 중
#define ACCESS_STATE_WAIT_FEW_SEC 2 // 통신과 통신간의 일정 시간 대기하는 상태.
#define ACCESS_STATE_IN_SETTING 3 // 모뎀 재설정 진행 중

// Interval of saving meter data into flash
#define METER_DATA_SAVE_INTERVAL 24
#if (METER_DATA_SAVE_INTERVAL <= 0)
#error "Meter data save interval have to be set above 0."
#endif

#define BASE_YEAR 2000
#define TIMER_1SEC 32768
#define TIMER_INTERVAL 25 // 타이머 갱신 주기

#define POWER_MODE LPM3_bits //RTC 동작시 파워모드
#define POWER_ACTIVE_OFF LPM2_bits //ACTIVE_OFF시 파워모드

#define WDT_ARST_16SEC (WDTPW + WDTCNTCL + WDTSSEL0 + WDTIS1 + WDTIS0) // Use ACLK, 16sec
#define WDT_VRST_3SEC (WDTPW + WDTCNTCL + WDTSSEL1 + WDTIS2) // Use VLOCLK, abour 3~4sec

//==============================================================================
// HAL data type
//==============================================================================
typedef unsigned char BOOL;
typedef unsigned char byte;
typedef unsigned char uchar;
typedef unsigned int word;
typedef unsigned long dword;
typedef unsigned long long qword;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned long uint32;
typedef unsigned long long uint64;

typedef uint32 event32_t;

typedef signed char int8;
typedef signed short int16;
typedef signed long int32;
typedef signed long long int64;

#ifdef NULL
#undef NULL
#endif

#ifdef FAIL
#undef FAIL
#endif

#define NULL 0
#define TRUE 1
#define FALSE 0
#define SUCCESS 1
#define FAIL 0
#define ON 1
#define OFF 0

#define SERIAL_NUM_LEN 12
#define SERVER_IP_STR_LEN 16 // string 길이이므로 \0 처리 길이까지 포함 시킴.

typedef struct {
	uint16 year; // yearH, yearL
	uint8 mon;
	uint8 day;
	uint8 hour;
	uint8 min;
	uint8 sec;
	uint8 reserved;
} Date_t;

typedef struct {
	uint8 serialNum[SERIAL_NUM_LEN + 1];
	uint8 meterType;
#if defined(AUX_REPEATER)
	uint16 pan_id;
	uint32 nwk_addr;
	uint32 slaveNwk;
	uint8 freqOffset;
	uint8 havePushButton;
#endif
#if LORA_DEVICE
	uint8 devEui[8];
#else
	char serviceCode[5]; // using only LG platfom. others all '0'
	char serverIp[SERVER_IP_STR_LEN];
	uint16 serverPort;
	char fotaIp[SERVER_IP_STR_LEN]; // using for FOTA. It used when not use LG platform.
	uint16 fotaPort; // using for FOTA. It used when not use LG platform.
	uint8 fotaInterval; // using for FOTA. It used when not use LG platform.
	uint8 imei[8];
	uint8 imsi[8];
	BOOL isModemInit;
#endif
	uint8 dataSkipMode;
	uint8 sleepMode;
	uint8 riCtrlMode; // 0 : no ctrl, 1 : ctrl
	uint8 riCtrlChgCount;
	uint8 riCtrlValue;
	uint8 isShortInterval;
	uint8 intervalBaseTime;
	uint8 meterInterval;
	uint8 reportInterval;
	uint8 reportRange;
	uint8 reportMin;
	uint8 reportSec;
	uint16 resetCount;
	uint8 debugPrint;
	uint8 termModel;
	uint8 periodMode;
	uint8 bslModel;
} Config_t;

extern Config_t conf;

//==============================================================================
//
//==============================================================================

#define SWAP32(x)                                                                                  \
	((((x) >> 24) & 0x000000FF) | (((x) >> 8) & 0x0000FF00) | (((x) << 8) & 0x00FF0000) |      \
	 (((x) << 24) & 0xFF000000))

#define SWAP16(x) ((((x) >> 8) & 0x00FF) | (((x) << 8) & 0xFF00))

/* takes a byte out of a uint32 : var - uint32,  ByteNum - byte to take out (0 - 3) */
#define BREAK_UINT32(var, ByteNum) (byte)((uint32)(((var) >> ((ByteNum)*8)) & 0x00FF))

#define BUILD_UINT32(Byte0, Byte1, Byte2, Byte3)                                                   \
	((uint32)((uint32)((Byte0)&0x00FF) + ((uint32)((Byte1)&0x00FF) << 8) +                     \
		  ((uint32)((Byte2)&0x00FF) << 16) + ((uint32)((Byte3)&0x00FF) << 24)))

#define BUILD_UINT16(loByte, hiByte) ((uint16)(((loByte)&0x00FF) + (((hiByte)&0x00FF) << 8)))

#define HI_UINT16(a) (((a) >> 8) & 0xFF)
#define LO_UINT16(a) ((a)&0xFF)

#define BUILD_UINT8(hiByte, loByte) ((uint8)(((loByte)&0x0F) + (((hiByte)&0x0F) << 4)))

#define HI_UINT8(a) (((a) >> 4) & 0x0F)
#define LO_UINT8(a) ((a)&0x0F)

extern BOOL device_sleep_state;

#define SET_ACTIVE_OFF_LPM(x)                                                                      \
	do {                                                                                       \
		if (x)                                                                             \
			__bis_SR_register(POWER_ACTIVE_OFF + GIE);                                 \
		else                                                                               \
			__bic_SR_register_on_exit(POWER_ACTIVE_OFF);                               \
	} while (0)

extern void LCD_disable();
extern void METER_disable();
extern void PRINT_disable();
extern void MODEM_disable();

#define SLEEP_DEVICE()                                                                             \
	do {                                                                                       \
		if (device_sleep_state == FALSE) {                                                 \
			P2OUT &= ~0x20;                                                            \
			TIMER_stop();                                                              \
			LCD_disable();                                                             \
			METER_disable();                                                           \
			PRINT_disable();                                                           \
			MODEM_disable();                                                           \
			WDTCTL = WDTPW | WDTHOLD;                                                  \
			device_sleep_state = TRUE;                                                 \
			__bis_SR_register(POWER_MODE + GIE);                                       \
		}                                                                                  \
		__no_operation();                                                                  \
	} while (0)

#define WAKEUP_DEVICE()                                                                            \
	do {                                                                                       \
		WDTCTL = WDT_ARST_16SEC;                                                           \
		if (device_sleep_state == TRUE) {                                                  \
			PRINT_enable();                                                            \
			TIMER_start();                                                             \
			device_sleep_state = FALSE;                                                \
			__bic_SR_register_on_exit(POWER_MODE);                                     \
		}                                                                                  \
		__no_operation();                                                                  \
	} while (0)

#ifndef ABS
#define ABS(n) (((n) < 0) ? -(n) : (n))
#endif

// Port 4
#define DIP0 0 // P1.0 - Input: DIP #0
#define DIP1 1 // P1.1 - Input: DIP #1
#define DIP2 2 // P1.2 - Input: DIP #2
#define DIP3 3 // P1.3 - Input: DIP #3
#define DIP4 4 // P1.4 - Input: DIP #4
#define DIP5 5 // P1.5 - Input: DIP #5
#define DIP6 6 // P1.6 - Input: DIP #6
#define DIP7 7 // P1.7 - Input: DIP #7

#define BM(x) (1 << x)

#define _BIT_SET(reg, n) ((reg) |= (1 << (n)))
#define _BIT_CLR(reg, n) ((reg) &= ~(1 << (n)))
#define _IS_SET(reg, n) ((reg) & (1 << (n)))
#define _IS_MIN(n, m) (((n) < (m)) ? (n) : (m))
#define _IS_MAX(n, m) (((n) < (m)) ? (m) : (n))
#define _ABS(n) (((n) < 0) ? (-(n)) : (n))

//==============================================================================

/*** Return Values ***/

#define ZSUCCESS 0
#define INVALID_TASK 1
#define MSG_BUFFER_NOT_AVAIL 2
#define INVALID_MSG_POINTER 3

typedef unsigned char halDataAlign_t;
typedef unsigned char halIntState_t;

/*
extern unsigned char IntsDisableCount; // count of performed disables
#define GIEDisable _DINT(); \
IntsDisableCount++;

#define GIEEnable if(--IntsDisableCount==0) \
_EINT();

#define GLOBAL_ENABLE_INT()          GIEEnable
#define GLOBAL_DISABLE_INT()        GIEDisable
*/

#define GLOBAL_ENABLE_INT() _EINT()
#define GLOBAL_DISABLE_INT() _DINT()
#define SYSTEM_RESET()                                                                             \
	do {                                                                                       \
		PMMCTL0 = (PMMPW + PMMSWPOR);                                                      \
	} while (0) /* force immediate system reset */
/*
#define HAL_ENTER_CRITICAL_SECTION(x)   GLOBAL_DISABLE_INT()
#define HAL_EXIT_CRITICAL_SECTION(x)   GLOBAL_ENABLE_INT()
*/

#define HAL_ENTER_CRITICAL_SECTION(x)                                                              \
	do {                                                                                       \
		x = __get_interrupt_state();                                                       \
		GLOBAL_DISABLE_INT();                                                              \
	} while (0)
#define HAL_EXIT_CRITICAL_SECTION(x)                                                               \
	do {                                                                                       \
		__set_interrupt_state(x);                                                          \
	} while (0)

#define NOP() __no_operation()

// System Event
#define SYS_EVENT_MSG 0x80000000

// flash address
#define FLASH_SEGA_ADDR 0x1980
#define FLASH_SEGB_ADDR 0x1900
#define FLASH_SEGC_ADDR 0x1880
#define FLASH_SEGD_ADDR 0x1800

#define SYSRSTIV_ADDR 0x019E // 0x0180 + 1E

#define ERROR_NONE 0
#define ERROR_REPORT 1
#define ERROR_METERING 2

#define TP_ANSI_RESET "\x1b[0m" // Foreground color table
#define TP_ANSI_BOLD_ON "\x1b[1m"
#define TP_ANSI_INVERSE_ON "\x1b[7m"
#define TP_ANSI_BOLD_OFF "\x1b[22m"
#define TP_ANSI_FG_BLACK "\x1b[30m"
#define TP_ANSI_FG_RED "\x1b[31m"
#define TP_ANSI_FG_GREEN "\x1b[32m"
#define TP_ANSI_FG_YELLOW "\x1b[33m"
#define TP_ANSI_FG_BLUE "\x1b[34m"
#define TP_ANSI_FG_MAGENTA "\x1b[35m"
#define TP_ANSI_FG_CYAN "\x1b[36m"
#define TP_ANSI_FG_WHITE "\x1b[37m"
#define TP_ANSI_BG_RED "\x1b[41m" // Background color table
#define TP_ANSI_BG_GREEN "\x1b[42m"
#define TP_ANSI_BG_YELLOW "\x1b[43m"
#define TP_ANSI_BG_BLUE "\x1b[44m"
#define TP_ANSI_BG_MAGENTA "\x1b[45m"
#define TP_ANSI_BG_CYAN "\x1b[46m"
#define TP_ANSI_BG_WHITE "\x1b[47m"

#define INT8_TYPE 8
#define INT16_TYPE 16

#endif

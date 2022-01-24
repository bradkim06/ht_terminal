#ifndef _OSAL_TIMER_H_
#define _OSAL_TIMER_H_

#include <msp430.h>
#include "common_header.h"

typedef struct {
	void *next;
	uint16 timeout;
	event32_t event_flag;
	uint8 taskId;
} OsalTimerRec_t;

#define UART1_TX_PIN 0x40
#define UART2_TX_PIN 0x10
#define LEN_TX_MSG 12 // meter conf set 12

typedef struct {
	uint8 len;
	uint8 bytePos;
	uint8 bitPos;
	uint8 txData;
	uint8 buf[LEN_TX_MSG];
} OsalTxBuf_t;

extern OsalTimerRec_t *OsalTimerHead;

void TIMER_init(void);
void TIMER_start(void);
void TIMER_stop(void);
uint32 TIMER_getMsec(void);
uint32 TIMER_getMsecDiff(uint32 ms);

BOOL OSAL_startEventTimer(byte taskID, event32_t event_id, uint32 timeout_value);
BOOL OSAL_stopEventTimer(uint8 taskId, event32_t event_id);
BOOL OSAL_setEvent(uint8 taskId, event32_t event_flag);

#endif // _OSAL_TIMER_H_
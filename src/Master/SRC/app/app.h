#ifndef _AMIAPP_HEADER_
#define _AMIAPP_HEADER_

#include "common_header.h"
#include "osal.h"
#include "flashDriver.h"

#define CONFIRM_NUMBER 0x13C5

///APP Event
#define APP_EVENT_INITIAL 0x00000001
#define APP_EVENT_RTC_ALARM 0x00000002
#define APP_EVENT_SENSOR_REED 0x00000004
#define APP_EVENT_METER_RX 0x00000008
#define APP_EVENT_METER_TIMEOUT 0x00000010
#define APP_EVENT_MODEM_PROCESS 0x00000020
#define APP_EVENT_MODEM_RX 0x00000040
#define APP_EVENT_MODEM_TIMEOUT 0x00000080
#define APP_EVENT_MODEM_POWER_OFF 0x00000100
#define APP_EVENT_MODEM_CHECK_ALIVE 0x00000200

#define APP_EVENT_FW_UPDATE     0x00000800
#define APP_EVENT_METER_ADJUST 0x00001000
#define APP_EVENT_NFC_WAIT 0x00002000
#define APP_EVENT_IMMEDIATE_METERING 0x00004000
#define APP_EVENT_IMMEDIATE_REPORT 0x00008000
#define APP_EVENT_USIM_RX 0x00010000
#define APP_EVENT_USIM_TIMEOUT 0x00020000
#define APP_EVENT_REBOOT 0x00040000
#define APP_EVENT_CHANGE_CONFIG 0x00080000
#define APP_EVENT_TEST 0x80000000

// APP Process
#define APP_IDLE 0
#define APP_INITIAL_REPORT 1
#define APP_PERIODIC_REPORT 2
#define APP_IMMEDIATE_REPORT 3
#define APP_SET_CONFIGURATION 4

extern uint8 AppTaskId;

// Function proto-type

void APP_init(uint8 taskId);
event32_t APP_tasks(uint8 taskId, event32_t events);
int APP_checkTimeInterval(int h, int bt, int interval);
void APP_prepareToSleep();

void REBOOT_SYSTEM(void);
void APP_runPeriodicCheckNFC();
void APP_runPeriodicReport();
void APP_runMetering(int meteringType);
void APP_showConfig(Config_t *p);

#if defined(AUX_REPEATER)
void APP_slaveAccessCompleted();
void APP_slaveAccessFailed();
void APP_slaveMeteringCompleted();
void APP_slaveMeteringFailed(int resultCode);
#endif

#endif

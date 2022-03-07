#include <msp430.h>
#include "common_header.h"
#include "Task_Mgr.h"
#include "osal_Timer.h"
#include "uart.h"
#include "Check_meter_misc.h"
#include "app.h"
#include "rtcAlarm.h"
#include "modem.h"
#include "flashDriver.h"
#if defined(AUX_REPEATER)
#include "cc1200.h"
#include "slaveAccess.h"
#endif

// The order in this table must be identical to the task initialization calls below in osalInitTask.

const pTaskEventHandlerFn TasksArray[] = {
	APP_tasks,
#if defined(AUX_REPEATER)
	CC1200_tasks,
	SLAVE_tasks,
#endif
};

const uint8 TotalTasksCnt = sizeof(TasksArray) / sizeof(TasksArray[0]);
event32_t *TasksEvents;

BOOL device_sleep_state;

void TASKMGR_init(void)
{
	uint8 taskID = 0;

	TasksEvents = (event32_t *)malloc(sizeof(event32_t) * TotalTasksCnt);
	if (TasksEvents == NULL) {
		printf("malloc error - TASKMGR_init\n");
		return;
	}

	memset(TasksEvents, 0, (sizeof(event32_t) * TotalTasksCnt));

	APP_init(taskID++);
#if defined(AUX_REPEATER)
	CC1200_init(taskID++);
	SLAVE_init(taskID++);
#endif
}

void TASKMGR_run(void)
{
	uint8 idx = 0;

	device_sleep_state = FALSE;

	for (;;) { // Forever Loop
		WDTCTL = WDTPW | WDTCNTCL; // Clear watchdog timer
		WDTCTL = WDT_ARST_16SEC; // Start watchdog timer(16 sec)

		if (++idx >= TotalTasksCnt) {
			idx = 0;
		}

		do {
			if (TasksEvents[idx]) { // Task is highest priority that is ready.
				break;
			}
		} while (++idx < TotalTasksCnt);

		if (idx < TotalTasksCnt) {
			halIntState_t intState;
			event32_t events;

			HAL_ENTER_CRITICAL_SECTION(intState); // Hold off interrupts
			events = TasksEvents[idx];
			TasksEvents[idx] = 0; // Clear the Events for this task.
			HAL_EXIT_CRITICAL_SECTION(intState); // Hold off interrupts

			events = (TasksArray[idx])(idx, events);

			HAL_ENTER_CRITICAL_SECTION(intState); // Hold off interrupts
			TasksEvents[idx] |=
				events; // Add back unprocessed events to the current task.
			HAL_EXIT_CRITICAL_SECTION(intState); // Hold off interrupts

		} else {
			if (OsalTimerHead == NULL) {
				int taskIdx;
				for (taskIdx = 0; taskIdx < TotalTasksCnt; taskIdx++) {
					if (TasksEvents[taskIdx] != 0x00000000) {
						break;
					}
				}

				if (taskIdx >= TotalTasksCnt) {
					APP_prepareToSleep();
					printf("--sleep--\n\n");
					MISC_delayMs(100);
					SLEEP_DEVICE();
				}
			}
		}
	}
}


#include <msp430.h>
#include "common_header.h"
#include "osal_Timer.h"
#include "port_desc.h"

#include "Task_Mgr.h"
#include "check_meter_misc.h"

OsalTimerRec_t *OsalTimerHead = NULL;

/*********************************************************************
 * @fn      findEventTimer
 *
 * @brief   Find a timer in a timer list.
 *          Ints must be disabled.
 *
 * @param   taskId
 * @param   event_flag
 *
 * @return  OsalTimerRec_t *
 */
static OsalTimerRec_t *findEventTimer(byte taskId, event32_t event_flag)
{
	OsalTimerRec_t *srchTimer;

	// Head of the timer list
	srchTimer = OsalTimerHead;

	// Stop when found or at the end
	while (srchTimer) {
		if (srchTimer->event_flag == event_flag && srchTimer->taskId == taskId)
			break;

		// Not this one, check another
		srchTimer = srchTimer->next;
	}

	return (srchTimer);
}

/*********************************************************************
 * @fn      delevtEventTimer
 *
 * @brief   Delete a timer from a timer list.
 *          Ints must be disabled.
 *
 * @param   table
 * @param   rmTimer
 *
 * @return  none
 */
static void delevtEventTimer(OsalTimerRec_t *rmTimer)
{
	OsalTimerRec_t *srchTimer;

	// Does the timer list really exist
	if ((OsalTimerHead != NULL) && rmTimer) {
		// Add it to the end of the timer list
		srchTimer = OsalTimerHead;

		// First element?
		if (srchTimer == rmTimer) {
			OsalTimerHead = rmTimer->next;
			free(rmTimer);
		} else {
			// Stop when found or at the end
			while (srchTimer->next && srchTimer->next != rmTimer)
				srchTimer = srchTimer->next;

			// Found?
			if (srchTimer->next == rmTimer) {
				// Fix pointers
				srchTimer->next = rmTimer->next;

				// Deallocate the timer struct memory
				free(rmTimer);
			}
		}
	}
}

/*********************************************************************
 * @fn      addEventTimer
 *
 * @brief   Add a timer to the timer list.
 *          Ints must be disabled.
 *
 * @param   taskId
 * @param   event_flag
 * @param   timeout
 *
 * @return  OsalTimerRec_t * - pointer to newly created timer
 */
static OsalTimerRec_t *addEventTimer(byte taskId, event32_t event_flag, uint16 timeout)
{
	OsalTimerRec_t *newTimer;
	OsalTimerRec_t *srchTimer;

	// Look for an existing timer first
	newTimer = findEventTimer(taskId, event_flag);
	if (newTimer) {
		// Timer is found - update it.
		newTimer->timeout = timeout;

		return (newTimer);
	} else {
		// New Timer
		newTimer = (OsalTimerRec_t *)malloc(sizeof(OsalTimerRec_t));

		//printf(">> (%d) 0x%x(%08lx)\n", taskId, newTimer, event_flag);
		if (newTimer) {
			// Fill in new timer
			newTimer->taskId = taskId;
			newTimer->event_flag = event_flag;
			newTimer->timeout = timeout;
			newTimer->next = (void *)NULL;

			// Does the timer list already exist
			if (OsalTimerHead == NULL) {
				// Start task list
				OsalTimerHead = newTimer;
			} else {
				// Add it to the end of the timer list
				srchTimer = OsalTimerHead;

				// Stop at the last record
				while (srchTimer->next)
					srchTimer = srchTimer->next;

				// Add to the list
				srchTimer->next = newTimer;

				//printf(" AD(%d): 0x%x(%08lx)\n", srchTimer->taskId, srchTimer, srchTimer->event_flag);
			}

			return (newTimer);
		} else {
			printf("malloc error - addEventTimer\n");
			return ((OsalTimerRec_t *)NULL);
		}
	}
}

/*********************************************************************
 * @fn      OSAL_startEventTimer
 *
 * @brief
 *
 *   This function is called to start a timer to expire in n mSecs.
 *   When the timer expires, the calling task will get the specified event.
 *
 * @param   byte taskID - task id to set timer for
 * @param   uint16 event_id - event to be notified with
 * @param   UNINT16 timeout_value - in milliseconds.
 *
 * @return  ZSUCCESS, or NO_TIMER_AVAIL.
 */
BOOL OSAL_startEventTimer(byte taskID, event32_t event_id, uint32 timeout_value)
{
	halIntState_t intState;
	OsalTimerRec_t *newTimer;
	uint16 timeout16 = (uint16)(timeout_value / TIMER_INTERVAL);
	if (timeout16 == 0) {
		timeout16 = 1;
	}

	HAL_ENTER_CRITICAL_SECTION(intState); // Hold off interrupts.
	// Add timer
	newTimer = addEventTimer(taskID, event_id, timeout16);

	HAL_EXIT_CRITICAL_SECTION(intState); // Re-enable interrupts.

	return ((newTimer != NULL) ? TRUE : FALSE);
}

/*********************************************************************
 * @fn      OSAL_stopEventTimer
 *
 * @brief
 *
 *   This function is called to stop a timer that has already been started.
 *   If ZSUCCESS, the function will cancel the timer and prevent the event
 *   associated with the timer from being set for the calling task.
 *
 * @param   byte taskId - task id of timer to stop
 * @param   uint16 event_id - identifier of the timer that is to be stopped
 *
 * @return  ZSUCCESS or INVALID_EVENT_ID
 */
BOOL OSAL_stopEventTimer(byte taskId, event32_t event_id)
{
	halIntState_t intState;
	OsalTimerRec_t *foundTimer;

	HAL_ENTER_CRITICAL_SECTION(intState); // Hold off interrupts

	// Find the timer to stop
	foundTimer = findEventTimer(taskId, event_id);
	if (foundTimer) {
		delevtEventTimer(foundTimer);
	}

	HAL_EXIT_CRITICAL_SECTION(intState); // Release interrupts

	return ((foundTimer != NULL) ? TRUE : FALSE);
}

static uint32 SysTimeMsec = 0;

uint32 TIMER_getMsec(void)
{
	return SysTimeMsec;
}

uint32 TIMER_getMsecDiff(uint32 ms)
{
	uint32 diff = 0;

	if (SysTimeMsec >= ms) {
		diff = SysTimeMsec - ms;
	} else {
		diff = 0xffffffff - ms + SysTimeMsec;
	}

	return diff;
}

void TIMER_init(void)
{
	OsalTimerHead = NULL;

	//	TA1CCTL0 = CCIE;                          // CCR0 interrupt enabled
	TA1CCR0 = TIMER_1SEC / (1000 / TIMER_INTERVAL);
	TA1CTL = TASSEL_1 + MC_1 + TACLR; // ACLK(32Khaz), Up mode, clear TAR

	SysTimeMsec = 0;
#if 0
    TA0R = 0;
    TA0CCR0 = TIMER_1SEC;
    TA0CTL = TASSEL_1 + MC_1 + TACLR;         // ACLK(32Khaz), Up mode, clear TAR
#endif
}

void TIMER_start(void)
{
	TA1CCTL0 = CCIE; // CCR0 interrupt enabled
}

void TIMER_stop(void)
{
	TA1CCTL0 &= ~CCIE; // CCR0 interrupt disable
}

// Timer1 A0 interrupt service routine
#pragma vector = TIMER1_A0_VECTOR
__interrupt void TIMER1_A0_ISR(void)
{
	SysTimeMsec += TIMER_INTERVAL;
	TA1R = 0;
	TA1CCR0 = TIMER_1SEC / (1000 / TIMER_INTERVAL);

	halIntState_t intState;
	OsalTimerRec_t *srchTimer;
	OsalTimerRec_t *prevTimer;
	OsalTimerRec_t *saveTimer;
	int updateTime = 1;

	HAL_ENTER_CRITICAL_SECTION(intState); // Hold off interrupts

	// Look for open timer slot
	if (OsalTimerHead != NULL) {
		// Add it to the end of the timer list
		srchTimer = OsalTimerHead;
		prevTimer = (void *)NULL;

		// Look for open timer slot
		while (srchTimer) {
			// Decrease the correct amount of time
			if (srchTimer->timeout <= updateTime)
				srchTimer->timeout = 0;
			else
				srchTimer->timeout = srchTimer->timeout - updateTime;

			// When timeout, execute the task
			if (srchTimer->timeout == 0) {
				OSAL_setEvent(srchTimer->taskId, srchTimer->event_flag);

				//Active ON시 Power Mode를 종료한다.
				SET_ACTIVE_OFF_LPM(FALSE);

				// Take out of list
				if (prevTimer == NULL)
					OsalTimerHead = srchTimer->next;
				else
					prevTimer->next = srchTimer->next;

				// Next
				saveTimer = srchTimer->next;

				// Free memory
				free(srchTimer);

				srchTimer = saveTimer;

			} else {
				// Get next
				prevTimer = srchTimer;
				srchTimer = srchTimer->next;
			}
		}
	}

	HAL_EXIT_CRITICAL_SECTION(intState); // Release interrupts
}

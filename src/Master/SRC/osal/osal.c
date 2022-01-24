
#include <msp430.h>
#include "common_header.h"

#include "Task_Mgr.h"
#include "osal.h"

// Message Pool Definitions
osal_msg_q_t osal_qHead;

/*********************************************************************
 * @fn      osal_msg_allocate
 *
 * @brief
 *
 *    This function is called by a task to allocate a message buffer
 *    into which the task will encode the particular message it wishes
 *    to send.  This common buffer scheme is used to strictly limit the
 *    creation of message buffers within the system due to RAM size
 *    limitations on the microprocessor.   Note that all message buffers
 *    are a fixed size (at least initially).  The parameter len is kept
 *    in case a message pool with varying fixed message sizes is later
 *    created (for example, a pool of message buffers of size LARGE,
 *    MEDIUM and SMALL could be maintained and allocated based on request
 *    from the tasks).
 *
 *
 * @param   byte len  - wanted buffer length
 *
 *
 * @return  pointer to allocated buffer or NULL if allocation failed.
 */
byte *osal_msg_allocate(uint16 len)
{
	OsalMsgHdr_t *hdr = NULL;

	if (len == 0)
		return (NULL);

	hdr = (OsalMsgHdr_t *)malloc((short)(len + sizeof(OsalMsgHdr_t)));
	if (hdr) {
		hdr->next = NULL;
		hdr->len = len;
		hdr->dest_id = TASK_NO_TASK;

#if defined(OSAL_TOTAL_MEM)
		osal_msg_cnt++;
#endif
		return ((byte *)(hdr + 1));
	} else {
		return (NULL);
	}
}

/*********************************************************************
 * @fn      osal_msg_deallocate
 *
 * @brief
 *
 *    This function is used to deallocate a message buffer. This function
 *    is called by a task (or processing element) after it has finished
 *    processing a received message.
 *
 *
 * @param   byte *msg_ptr - pointer to new message buffer
 *
 * @return  ZSUCCESS, INVALID_MSG_POINTER
 */
byte osal_msg_deallocate(byte *msg_ptr)
{
	byte *x;

	if (msg_ptr == NULL)
		return (INVALID_MSG_POINTER);

	// don't deallocate queued buffer
	if (OSAL_MSG_ID(msg_ptr) != TASK_NO_TASK)
		return (MSG_BUFFER_NOT_AVAIL);

	x = (byte *)((byte *)msg_ptr - sizeof(OsalMsgHdr_t));

	free((void *)x);

	return (ZSUCCESS);
}

/*********************************************************************
 * @fn      osal_msg_send
 *
 * @brief
 *
 *    This function is called by a task to send a command message to
 *    another task or processing element.  The sending_task field must
 *    refer to a valid task, since the task ID will be used
 *    for the response message.  This function will also set a message
 *    ready event in the destination tasks event list.
 *
 *
 * @param   byte destination task - Send msg to?  Task ID
 * @param   byte *msg_ptr - pointer to new message buffer
 * @param   byte len - length of data in message
 *
 * @return  ZSUCCESS, INVALID_SENDING_TASK, INVALID_DESTINATION_TASK,
 *          INVALID_MSG_POINTER, INVALID_LEN
 */
byte osal_msg_send(uint8 destination_task, byte *msg_ptr)
{
	if (msg_ptr == NULL)
		return (INVALID_MSG_POINTER);

	if (destination_task >= TotalTasksCnt) {
		osal_msg_deallocate(msg_ptr);
		return (INVALID_TASK);
	}

	// Check the message header
	if (OSAL_MSG_NEXT(msg_ptr) != NULL || OSAL_MSG_ID(msg_ptr) != TASK_NO_TASK) {
		osal_msg_deallocate(msg_ptr);
		return (INVALID_MSG_POINTER);
	}

	OSAL_MSG_ID(msg_ptr) = destination_task;

	// queue message
	osal_msg_enqueue(&osal_qHead, msg_ptr);

	// Signal the task that a message is waiting
	OSAL_setEvent(destination_task, SYS_EVENT_MSG);

	return (ZSUCCESS);
}

/*********************************************************************
 * @fn      osal_msg_receive
 *
 * @brief
 *
 *    This function is called by a task to retrieve a received command
 *    message. The calling task must deallocate the message buffer after
 *    processing the message using the osal_msg_deallocate() call.
 *
 * @param   byte taskId - receiving tasks ID
 *
 * @return  *byte - message information or NULL if no message
 */
byte *osal_msg_receive(byte taskId)
{
	OsalMsgHdr_t *listHdr;
	OsalMsgHdr_t *prevHdr = 0;
	halIntState_t intState;

	// Hold off interrupts
	HAL_ENTER_CRITICAL_SECTION(intState);

	// Point to the top of the queue
	listHdr = osal_qHead;

	// Look through the queue for a message that belongs to the asking task
	while (listHdr != NULL) {
		if ((listHdr - 1)->dest_id == taskId) {
			break;
		}
		prevHdr = listHdr;
		listHdr = OSAL_MSG_NEXT(listHdr);
	}

	// Did we find a message?
	if (listHdr == NULL) {
		// Release interrupts
		HAL_EXIT_CRITICAL_SECTION(intState);
		return NULL;
	}

	// Take out of the link list
	osal_msg_extract(&osal_qHead, listHdr, prevHdr);

	// Release interrupts
	HAL_EXIT_CRITICAL_SECTION(intState);

	return ((byte *)listHdr);
}

/*********************************************************************
 * @fn      osal_msg_enqueue
 *
 * @brief
 *
 *    This function enqueues an OSAL message into an OSAL queue.
 *
 * @param   osal_msg_q_t *q_ptr - OSAL queue
 * @param   void *msg_ptr  - OSAL message
 *
 * @return  none
 */
void osal_msg_enqueue(osal_msg_q_t *q_ptr, void *msg_ptr)
{
	void *list;
	halIntState_t intState;

	// Hold off interrupts
	HAL_ENTER_CRITICAL_SECTION(intState);

	// If first message in queue
	if (*q_ptr == NULL) {
		*q_ptr = msg_ptr;
	} else {
		// Find end of queue
		for (list = *q_ptr; OSAL_MSG_NEXT(list) != NULL; list = OSAL_MSG_NEXT(list))
			;

		// Add message to end of queue
		OSAL_MSG_NEXT(list) = msg_ptr;
	}

	// Re-enable interrupts
	HAL_EXIT_CRITICAL_SECTION(intState);
}

/*********************************************************************
 * @fn      osal_msg_dequeue
 *
 * @brief
 *
 *    This function dequeues an OSAL message from an OSAL queue.
 *
 * @param   osal_msg_q_t *q_ptr - OSAL queue
 *
 * @return  void * - pointer to OSAL message or NULL of queue is empty.
 */
void *osal_msg_dequeue(osal_msg_q_t *q_ptr)
{
	void *msg_ptr;
	halIntState_t intState;

	// Hold off interrupts
	HAL_ENTER_CRITICAL_SECTION(intState);

	if (*q_ptr == NULL) {
		HAL_EXIT_CRITICAL_SECTION(intState);
		return NULL;
	}

	// Dequeue message
	msg_ptr = *q_ptr;
	*q_ptr = OSAL_MSG_NEXT(msg_ptr);
	OSAL_MSG_NEXT(msg_ptr) = NULL;
	OSAL_MSG_ID(msg_ptr) = TASK_NO_TASK;

	// Re-enable interrupts
	HAL_EXIT_CRITICAL_SECTION(intState);

	return msg_ptr;
}

/*********************************************************************
 * @fn      osal_msg_extract
 *
 * @brief
 *
 *    This function extracts and removes an OSAL message from the
 *    middle of an OSAL queue.
 *
 * @param   osal_msg_q_t *q_ptr - OSAL queue
 * @param   void *msg_ptr  - OSAL message to be extracted
 * @param   void *prev_ptr  - OSAL message before msg_ptr in queue
 *
 * @return  none
 */
void osal_msg_extract(osal_msg_q_t *q_ptr, void *msg_ptr, void *prev_ptr)
{
	halIntState_t intState;

	// Hold off interrupts
	HAL_ENTER_CRITICAL_SECTION(intState);

	if (msg_ptr == *q_ptr) {
		// remove from first
		*q_ptr = OSAL_MSG_NEXT(msg_ptr);
	} else {
		// remove from middle
		OSAL_MSG_NEXT(prev_ptr) = OSAL_MSG_NEXT(msg_ptr);
	}
	OSAL_MSG_NEXT(msg_ptr) = NULL;
	OSAL_MSG_ID(msg_ptr) = TASK_NO_TASK;

	// Re-enable interrupts
	HAL_EXIT_CRITICAL_SECTION(intState);
}

/*********************************************************************
 * @fn      OSAL_setEvent
 *
 * @brief
 *
 *    This function is called to set the event flags for a task.  The
 *    event passed in is OR'd into the task's event variable.
 *
 * @param   byte taskId - receiving tasks ID
 * @param   byte event_flag - what event to set
 *
 * @return  ZSUCCESS, INVALID_TASK
 */
BOOL OSAL_setEvent(uint8 taskId, event32_t event_flag)
{
	halIntState_t intState;

	if (taskId < TotalTasksCnt) {
		HAL_ENTER_CRITICAL_SECTION(intState); // Hold off interrupts
		TasksEvents[taskId] |= event_flag; // Stuff the event bit(s)
		HAL_EXIT_CRITICAL_SECTION(intState); // Release interrupts
	} else
		return (FALSE);

	return (TRUE);
}

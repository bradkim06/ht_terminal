
#ifndef OSAL_HEADER
#define OSAL_HEADER

#include "common_header.h"

#define TASK_NO_TASK 0xFF

/*********************************************************************
 * MACROS
 */

#define osal_offsetof(type, member) ((uint16) & (((type *)0)->member))

#define OSAL_MSG_NEXT(msg_ptr) ((OsalMsgHdr_t *)(msg_ptr)-1)->next

#define OSAL_MSG_Q_INIT(q_ptr) *(q_ptr) = NULL

#define OSAL_MSG_Q_EMPTY(q_ptr) (*(q_ptr) == NULL)

#define OSAL_MSG_Q_HEAD(q_ptr) (*(q_ptr))

#define OSAL_MSG_LEN(msg_ptr) ((OsalMsgHdr_t *)(msg_ptr)-1)->len

#define OSAL_MSG_ID(msg_ptr) ((OsalMsgHdr_t *)(msg_ptr)-1)->dest_id

/*********************************************************************
 * TYPEDEFS
 */
typedef struct {
	void *next;
	uint16 len;
	byte dest_id;
} OsalMsgHdr_t;

typedef struct {
	uint8 event;
	uint8 status;
} OsalEventHdr_t;

typedef void *osal_msg_q_t;

typedef struct {
	OsalEventHdr_t hdr;
	uint32 srcAddr;
	uint32 destAddr;
	uint16 len;
	byte *msg;
} OsalMsg_t;

/*********************************************************************
 * FUNCTIONS
 */

//void *memcpy( void *dst, const void *src, unsigned int len );

/*
   * Task Message Allocation
   */
extern byte *osal_msg_allocate(uint16 len);

/*
   * Task Message Deallocation
   */
extern byte osal_msg_deallocate(byte *msg_ptr);

/*
   * Task Messages Count
   */
extern uint16 osal_num_msgs(void);

/*
   * Send a Task Message
   */
extern byte osal_msg_send(byte destination_task, byte *msg_ptr);

/*
   * Receive a Task Message
   */
extern byte *osal_msg_receive(byte taskId);

/*
   * Enqueue a Task Message
   */
extern void osal_msg_enqueue(osal_msg_q_t *q_ptr, void *msg_ptr);

/*
   * Dequeue a Task Message
   */
extern void *osal_msg_dequeue(osal_msg_q_t *q_ptr);

/*
   * Extract and remove a Task Message from queue
   */
extern void osal_msg_extract(osal_msg_q_t *q_ptr, void *msg_ptr, void *prev_ptr);

extern BOOL OSAL_setEvent(uint8 taskId, event32_t event_flag);

#endif

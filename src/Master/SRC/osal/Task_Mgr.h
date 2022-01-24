#ifndef _TASK_MGR_H_
#define _TASK_MGR_H_

/*********************************************************************
 * TYPEDEFS
 */

/*
 * Event handler function prototype
 */

typedef event32_t (*pTaskEventHandlerFn)(uint8 taskId, event32_t event);

/*********************************************************************
 * GLOBAL VARIABLES
 */

extern const pTaskEventHandlerFn TasksArray[];
extern const uint8 TotalTasksCnt;
extern event32_t *TasksEvents;

void TASKMGR_init(void);
void TASKMGR_run(void);

#endif // _TASK_MGR_H_
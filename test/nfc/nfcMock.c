#include "nfcMock.h"
#include "nfcProtocol.h"
#include "tdd.h"

extern nfcExpect_t result;

// Mock
BOOL OSAL_setEvent(uint8 taskId, event32_t event_flag)
{
	result.osalEvent = event_flag;
	if (event_flag == APP_EVENT_CHANGE_CONFIG) {
		printf("OSAL_setEvent Run, APP_EVENT_CHANGE_CONFIG\n");
	}

	return 0;
}
void UART_debugMode()
{
}
void PRINT_resume()
{
}
void PRINT_enable()
{
}
void UART_init()
{
}
void send(byte *p, int len)
{
	memcpy(&result.msg, p, sizeof(result.msg));
	result.sendLen = len;
}

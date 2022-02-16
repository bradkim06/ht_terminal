#ifndef __MOCK_NFC_H__
#define __MOCK_NFC_H__

#include "app.h"
#include "nfcProtocol.h"

typedef struct {
	event32_t osalEvent;
	Config_t conf;
	NfcBdCtrlAckV5_t msg;
	int sendLen;
} nfcExpect_t;

typedef struct {
	char testName[100];
	NfcBdCtrlReqV5_t data;
	nfcExpect_t expect;
} bdReqInput_t;

#endif

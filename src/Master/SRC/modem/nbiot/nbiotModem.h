#ifndef NBIOT_LG_TYPE
#ifndef __NBIOT_MODEM_H__
#define __NBIOT_MODEM_H__

#include "modem.h"
#include "message.h"

// NB-IoT Modem AT command index list
#define AT_CMD_IDX_CHECK_ALIVE 1
#define AT_CMD_IDX_SW_RESET 2
#define AT_CMD_IDX_DETACH_NW 3
#define AT_CMD_IDX_GET_IMEI 4
#define AT_CMD_IDX_GET_IMSI 5
#define AT_CMD_IDX_GET_ICCID 6
#define AT_CMD_IDX_GET_RADIO_QUALITY 7
#define AT_CMD_IDX_GET_NW_ALARM 8
#define AT_CMD_IDX_GET_TIME 9
#define AT_CMD_IDX_GET_CDP 10
#define AT_CMD_IDX_GET_EPN 11
#define AT_CMD_IDX_GET_BSPS 12
#define AT_CMD_IDX_SET_RF_CTRL 13
#define AT_CMD_IDX_SET_PSM 14
#define AT_CMD_IDX_SET_BAND 15
#define AT_CMD_IDX_SET_RESELECT 16
#define AT_CMD_IDX_SET_CDP 17
#define AT_CMD_IDX_SET_EPN 18
#define AT_CMD_IDX_SET_BSPS 19
#define AT_CMD_IDX_SET_FOTA 20
#define AT_CMD_IDX_SET_LWM2M 21
#define AT_CMD_IDX_SET_NW_ALARM 22
#define AT_CMD_IDX_RUN_BOOTSTRAP 23
#define AT_CMD_IDX_RUN_REGISTER 24
#define AT_CMD_IDX_RUN_DATA_NOTI 25
#define AT_CMD_IDX_SOCKET_CREATE 26
#define AT_CMD_IDX_SOCKET_SEND_UL 27
#define AT_CMD_IDX_SOCKET_RECV_DL 28
#define AT_CMD_IDX_SOCKET_CLOSE 29
#define AT_CMD_IDX_GET_FW_REV 30
#define AT_CMD_IDX_CHECK_BIPST 31
#define AT_CMD_IDX_SET_REPORT_PSM 32
#define AT_CMD_IDX_SET_LWM2M_SERVER 33 // >> AT_CMD_IDX_SET_CDP
#define AT_CMD_IDX_GET_LWM2M_SERVER 34 // >> AT_CMD_IDX_GET_CDP
#define AT_CMD_IDX_SWITCH_LWM2M 35
#define AT_CMD_IDX_GET_SWITCH_LWM2M 36

#define MODEM_CELLREG_NOT_REG 0
#define MODEM_CELLREG_ATTACHED 1
#define MODEM_CELLREG_PAUSE 2
#define MODEM_CELLREG_REJECTED 3
#define MODEM_CELLREG_NO_SERVICE 4

// Modem access step
typedef enum {
	MODEM_STEP_IDLE,
	MODEM_STEP_BIP,
	MODEM_STEP_INIT, // Initialize step
	MODEM_STEP_ATTACH_NW, // Connecting N/W
	MODEM_STEP_TRANSFER, // Send UDP uplink
	MODEM_STEP_CERTIFY,
	MODEM_STEP_FOTA,
	MODEM_STEP_UPDATE_QA,
	MODEM_STEP_DETACH_NW,
	MODEM_STEP_RETRY, // MODEM step retry operation
	MODEM_STEP_UNKNOWN = 0xFF
} ModemStep_t;

// Modem access step
typedef enum {
	MODEM_ERROR_NONE = 0,
	MODEM_ERROR_USIM_INVALID = 0x11, // AT+CIMI 무응답 OR ERROR
	MODEM_ERROR_PF_EPNS_CHANGE = 0x12, // EPNS 변경 발생.
	MODEM_ERROR_PF_CERITY_FAIL = 0x13, // Bootstrap or Register 실패
	MODEM_ERROR_PF_UL_FAIL = 0x14, // Platform 연동 시 UL 실패
	MODEM_ERROR_PF_UD_FAIL = 0x15, // Platform 연동 시 DL 실패
	MODEM_ERROR_PF_FOTA_FAIL = 0x16, // Platform module FOTA 실패
	MODEM_ERROR_ATTACH_FAIL = 0x21, // 망접속 실패
	MODEM_ERROR_UDP_UL_FAIL = 0x31, // UDP UL 실패
	MODEM_ERROR_UDP_DL_FAIL = 0x32, // UDP DL 실패
	MODEM_ERROR_AT_CMD_NO_RESP = 0x35, // AT command no response
	MODEM_ERROR_AT_CMD_FAIL = 0x36, // AT command error
} ModemErrorCode_t;

typedef union {
	uint8 value; // Flag for reset
	struct {
		uint8 error : 1, // AT command Error 여부를 표시. (모뎀 시작/종료/AT command 전송 시 초기화.)
			busy : 1, // AT command 전송 여부를 표시. (Response를 전송받거나 Timeout, 강제 전송 시 초기화.)
			stop : 1, // 동작 여부를 표시. (시작 시 설정, 종료 시 초기화)
			cellreg : 3, // 네트워크 접속 상태를 표시. (Power off, reboot, detach 시 초기화)
			psmOn : 1, // PSM 동작 여부를 표시. (기능 여부 및 동작에 따라 설정 및 초기화)
			lwm2mOn : 1; // LWM2M 동작여부를 표시.
	};
} ModemStatus_t;

typedef union {
	uint8 value; // Flag for reset
	struct {
		uint8 runInit : 1, // Flag to set whether MODEM configuration is necessary in INIT step
			hwRstRetry : 1, // Flag to set whether H/W reset is necessary in RETRY step
			updateReg : 1, // Flag to set whether Register Update is necessary in CERTIFY step
			updateQa : 1, // Flag to set whether Update QA is necessary after attached
			runFota : 1, // Flag to set whether to check FOTA request.
			reserved : 3;
	};
} ModemProc_t;

typedef union {
	uint16 value; // Flag for reset
	struct {
		uint16 bsFinish : 1, // Finish Bootstrap
			regFinish : 1, // Finish Register
			regUpdate : 1, // Update Register
			regDelete : 1, // Delete Register
			obsObj10250 : 1, // Observe object 10250
			obsObj503 : 1, // Observe object 503
			obsObj16241 : 1, // Observe object 16241
			cxlObj10250 : 1, // Cancel object 10250
			fotaDownReq : 1, // FOTA download requested
			fotaUpgradeReq : 1, // FOTA download requested
			fotaFinish : 1, // Finish FOTA
			reserved : 5;
	};
} ModemLwm2m_t;

typedef struct {
	uint8 index; // pre-defined AT command index
	char *cmd; // AT command (ASCII)
} AtCmd_t;

typedef struct {
	int retry; // command retry count
	uint32 timeout; // command working timeout
	AtCmd_t *atCmd; // AT command
	char atData[LEN_MAX_AT_DATA]; // AT data(=command + parameter, ASCII)
	int dlDataLen; // Downlink data length
	char dlData[LEN_MAX_DL_DATA]; // Downlink data
} ModemComm_t;

#define LEN_ERROR_LOG LEN_NBIOT_ERR_E_LOG
#define CNT_ERROR_LOG CNT_NBIOT_ERR_E_LOG

typedef struct {
	ModemStatus_t status; // 상태정보. 필요할 때만 설정 및 초기화. 자세한 사항은 구조체 정의 참조.
	ModemProc_t proc; // 특정 동작여부를 설정. IDLE에서 설정, 동작 완료 시 초기화.
	ModemLwm2m_t
		lwm2m; // LWM2M 이벤트 표시. 이벤트 발생 시 설정, power off 및 attahc 실패 시 clear.
	ModemStep_t step;
	ModemStep_t retryStep;
	uint8 retryCount;
	ModemErrorCode_t errCode;
	int socket;
	BOOL stepReset;
	int ulCnt;
	int dlCnt;
	int pfUlCnt;
	int pfDlCnt;
	uchar errLog[CNT_ERROR_LOG][LEN_ERROR_LOG];
} ModemContext_t;

extern ModemComm_t modemComm;
extern ModemContext_t modemCtx;

#define MODEM_STEP_STRING(x)                                                                       \
	(((x) == MODEM_STEP_IDLE) ?                                                                \
		 "IDLE" :                                                                          \
		 ((x) == MODEM_STEP_BIP) ?                                                         \
		 "BIP" :                                                                           \
		 ((x) == MODEM_STEP_INIT) ?                                                        \
		 "INIT" :                                                                          \
		 ((x) == MODEM_STEP_ATTACH_NW) ?                                                   \
		 "ATTACH NW" :                                                                     \
		 ((x) == MODEM_STEP_DETACH_NW) ?                                                   \
		 "DETACH NW" :                                                                     \
		 ((x) == MODEM_STEP_FOTA) ?                                                        \
		 "FOTA" :                                                                          \
		 ((x) == MODEM_STEP_UPDATE_QA) ?                                                   \
		 "UPDATE QA" :                                                                     \
		 ((x) == MODEM_STEP_CERTIFY) ?                                                     \
		 "CERTIFY" :                                                                       \
		 ((x) == MODEM_STEP_TRANSFER) ? "TRANSFER" :                                       \
						((x) == MODEM_STEP_RETRY) ? "RETRY" : "UNKNOWN")

void MODEM_detach();
BOOL MODEM_process();
void MODEM_response(char *pHead, int len);

#endif // __NBIOT_MODEM_H__
#endif

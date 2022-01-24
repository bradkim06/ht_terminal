#ifndef _LORA_MODEM_H_
#define _LORA_MODEM_H_

#define MODEM_STEP_STRING(x)                                                                       \
	(((x) == MODEM_STEP_IDLE) ? "IDLE" :                                                       \
				    ((x) == MODEM_STEP_CHECK_ALIVE) ?                              \
				    "CHECK ALIVE" :                                                \
				    ((x) == MODEM_STEP_GET_NWK_SESSION) ?                          \
				    "GET NWK" :                                                    \
				    ((x) == MODEM_STEP_SET_NWK_SESSION) ?                          \
				    "SET NWK" :                                                    \
				    ((x) == MODEM_STEP_WAIT_PROVISION) ?                           \
				    "PROVISION" :                                                  \
				    ((x) == MODEM_STEP_WAIT_ACK_DATA) ?                            \
				    "DATA" :                                                       \
				    ((x) == MODEM_STEP_GET_SIG) ?                                  \
				    "SIGNAL" :                                                     \
				    ((x) == MODEM_STEP_GET_TIME) ? "TIME SYNC" : "UNKNOWN")

#define MODEM_TX_RETRY_COUNT 8

typedef enum {
	ATCMD_IDX_CHECK_ALIVE = 1,
	ATCMD_IDX_SET_DR = 2,
	ATCMD_IDX_SET_ADR = 3,
	ATCMD_IDX_SEND_DATA = 4,
	ATCMD_IDX_GET_DEVEUI = 5,
	ATCMD_IDX_GET_APPEUI = 6,
	ATCMD_IDX_GET_APPKEY = 7,
	ATCMD_IDX_SET_APPEUI = 8,
	ATCMD_IDX_SET_APPKEY = 9,
	ATCMD_IDX_GET_SIG = 10,
	ATCMD_IDX_GET_TIME = 11,
	ATCMD_IDX_SAVE_CFG = 12,
	ATCMD_IDX_RESET_NWK = 13,
	ATCMD_IDX_SW_RESET = 14,
} AtCmdIdx_t;

// Modem error code
typedef enum {
	MODEM_ERROR_NONE = 0,
	MODEM_ERROR_NOT_ACTIVE = 1,
	MODEM_ERROR_JOIN_FAIL = 2,
	MODEM_ERROR_DATA_FAIL = 3,
	MODEM_ERROR_NO_RESP = 4,
	MODEM_ERROR_TIME_SYNC_FAIL = 5,
	MODEM_ERROR_UNKNOWN = 255
} ModemError_t;

// Modem access step
typedef enum {
	MODEM_STEP_IDLE = 0,
	MODEM_STEP_CHECK_ALIVE = 1,
	MODEM_STEP_GET_NWK_SESSION = 2,
	MODEM_STEP_SET_NWK_SESSION = 3,
	MODEM_STEP_WAIT_PROVISION = 4,
	MODEM_STEP_WAIT_ACK_DATA = 5,
	MODEM_STEP_GET_SIG = 6,
	MODEM_STEP_GET_TIME = 7,
	MODEM_STEP_UNKNOWN = 255
} ModemStep_t;

typedef union {
	uint8 value; // Flag for reset
	struct {
		uint8 alive : 1, // 모뎀 동작 여부.
			stop : 1, // 모뎀 동작 완료 여부. (쓰레기 값에 의한 오동작 방지를 위한 Flag)
			getSig : 1, // 통신 완료 후 Signal quality 확인 여부.
			timeSync : 1, // 시간동기화 완료 여부.
			provision : 1, // Join 완료 여부.
			acked : 1, // Data 전송 및 응답 완료 여부.
			busy : 1, // AT command 전송 및 응답 완료 여부.
			timeout : 1; // Data 전송 중 Timeout 발생 여부.
	};
} LoraStatus_t;

typedef union {
	uint8 value; // Flag for reset
	struct {
		uint8 getDevEui : 1, // Dev EUI 확인 여부.
			getAppEui : 1, // App EUI 확인 여부.
			getAppKey : 1, // App KeyEUI 확인 여부.
			setAppEui : 1, // App EUI 설정 여부.
			setAppKey : 1, // App KeyEUI 설정 여부.
			saveCfg : 1, // 설정된 값 저장 여부.
			resetNwk : 1, // Network session 리셋
			setAdr : 1, // ADR 설정 여부.
			setDr : 1; // Data rate 설정 여부.
	};
} LoraConfig_t;

typedef struct {
	AtCmdIdx_t index;
	char *cmd;
} AtCmd_t;

typedef struct {
	int retry; // command retry count
	uint32 timeout; // command working timeout
	const AtCmd_t *lastAtCmd;
	char atData[MODEM_MAX_AT_CMD_LEN]; // AT data(=command + parameter, ASCII)
} ModemComm_t;

typedef struct {
	int dataTimeoutCnt;
	int errorCnt;
	ModemError_t errCode;
	ModemStep_t step;
	LoraStatus_t status;
	LoraConfig_t config;
	Date_t closeTime;
	Date_t joinTime;
} ModemContext_t;

// Modem context는 모뎀 동작상 필요한 context들의 집합
extern ModemContext_t modemCtx;
extern ModemComm_t modemComm;

BOOL MODEM_process(int appProcess);
void MODEM_response(char *pHead, int len);

#endif // _LORA_MODEM_H_

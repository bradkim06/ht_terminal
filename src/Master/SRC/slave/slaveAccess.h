#ifndef __RF424_H__
#define __RF424_H__

#include "app.h"

#define SLAVE_NONE 0
#define SLAVE_FAIL -1
#define SLAVE_SUCCESS 1

//---------------------------------------------------------------------
#define DEFAULT_SINK_ADDR 0x00000000 // 수집기 주소

#define DEFAULT_PDA_PANID 0x0FFF // Default PDA Pan id
#define DEFAULT_PDA_ADDR 0xFBFBFB00 // 251.251.251.0
#define PDA_MODE_BASIC_PANID 0x0000 // 기존 PDA 모드 PANID
#define DEFAULT_GROUP_SCAN_ADDR 0xFCFCFC00 //252.252.252.0
#define BROADCAST_ADDR 0xFFFFFFFF
//---------------------------------------------------------------------

#define LEN_SLAVE_SERIAL_NUM 10
#define LEN_METER_SERIAL_NUM 4
#define LEN_METER_DATA 4
#define LEN_NETWORK_ADDRESS 4

#define INTERVAL_HOUR_AUTO_UPDATE_METER_INFO 24 // Slave meter infomation update interval
#define INTERVAL_SEC_SLAVE_ACCESS 10 // Slave access interval for external interface

typedef enum {
	SLAVE_ACCESS_IDLE = 0,
	SLAVE_ACCESS_READ_METER,
	SLAVE_ACCESS_GET_SLAVE_INFO,
	SLAVE_ACCESS_GET_METER_INFO,
	SLAVE_ACCESS_GET_DATA,
	SLAVE_ACCESS_SET_TIME_SYNC,
} SlaveAccessState_t;

typedef struct {
	char slaveSerial[LEN_SLAVE_SERIAL_NUM + 1];
	uint8 slaveBatt;
	uint8 slaveRssi;
	uint8 masterRssi;
	uint8 lastError;
} SlaveAccessInfo_t;

typedef enum {
	SLAVE_METERING_IDLE = 0,
	SLAVE_METERING_STARTED,
	SLAVE_METERING_OK,
	SLAVE_METERING_MT_DOWN,
	SLAVE_METERING_COMM_ERR,
} slaveMeteringResult_t;

//=======================================
// Events
//=======================================
#define SLAVE_EVENT_SLAVE_ACCESS_TIMEOUT 0x00000001
#define SLAVE_EVENT_SET_TIME_SYNC 0x00000002
#define SLAVE_EVENT_GET_SLAVE_INFO 0x00000004
#define SLAVE_EVENT_GET_METER_INFO 0x00000008
#define SLAVE_EVENT_SLAVE_ACCESS_CLOSE 0x00000010

//=======================================
// AMI master <--> AMI slave간  Message ID
//=======================================
// Slave 검침 실패 - 주기 검침에 대해 검침 실패를 알려주는 메시지
#define MSG_AMI_NODE_EVENT_ALARM 0x04

// Slave가 미터를 즉시 읽도록 지시하기
#define MSG_PDA_GROUP_SCAN_REQ 0x1E // master --> slave

// Slave 장치 상태 확인 정보 읽어 오기
#define MSG_AMI_MASTER_SLAVE_CHECK_REQ 0x47 // master --> slave
#define MSG_AMI_MASTER_SLAVE_CHECK_REPORT 0x48 // slave  --> master

// 현 지침값, 미터 정보 읽어 오기
#define MSG_PDA_MASTER_SLAVE_METER_REQ 0x49 // master --> slave
#define MSG_PDA_GROUP_DATA_REPORT 0x1A // slave  --> master

// 메모리에 저장된 누적 데이터 읽어 오기
#define MSG_AMI_MASTER_SLAVE_METER_REQ 0x42 // master --> slave
#define MSG_AMI_MULTI_DATA_REPORT 0x25 // slave  --> master
#define MSG_AMI_MULTI_DATA_ACK 0x26 // master --> slave

#define MSG_PDA_NODE_CONF_SUCCESS 0x10

//=======================================
// RF424 Message Type
//=======================================

typedef struct {
	uint8 mtype;
	uint8 year[2];
	uint8 mon;
	uint8 day;
	uint8 hour;
	uint8 min;
	uint8 sec;
	uint8 waitSec; // 1~255 sec
} PdaGroupScanReq_t;

typedef struct {
	uint8 mtype;
	uint8 year[2];
	uint8 mon;
	uint8 day;
	uint8 hour;
	uint8 min;
	uint8 sec;
} AmiMasterSlaveMeterReq_t;

typedef struct {
	uint8 mtype;
	uint8 serialNum[LEN_SLAVE_SERIAL_NUM];
	uint8 nwkAddr[LEN_NETWORK_ADDRESS];
	uint8 nodeType;
	uint8 batt;
	uint8 rssi;
} AmiMasterSlaveCheckReport_t;

typedef struct {
	uint8 mtype;
} PdaMasterSlaveMeterReq_t;

typedef PdaMasterSlaveMeterReq_t AmiMasterSlaveCheckReq_t;

typedef struct {
	uint8 mtype;
	uint8 nwkAddr[LEN_NETWORK_ADDRESS];
	uint8 reqType;
	uint8 moreFlag;
	uint8 meterType;
	uint8 dif;
	uint8 vif;
	uint8 year[2];
	uint8 mon;
	uint8 day;
	uint8 hour;
	uint8 min;
	uint8 meterSerial[LEN_METER_SERIAL_NUM];
	uint8 unused[6];
	uint8 meterData[LEN_METER_DATA];
	uint8 batt; // bit 6-7: meter battery, bit 0-5: terminal battery
	uint8 status;
	uint8 tempHour;
	uint8 tempData;
} PdaGroupDataReportStd_t;

typedef struct {
	uint8 mtype;
	uint8 nwkAddr[LEN_NETWORK_ADDRESS];
	uint8 reqType;
	uint8 moreFlag;
	uint8 meterType;
	uint8 year[2];
	uint8 mon;
	uint8 day;
	uint8 hour;
	uint8 min;
	uint8 meterSerial[LEN_METER_SERIAL_NUM];
	uint8 unused[6]; // m&s의 경우에는 unused 없이 10바이트가 모두 meter serial no(ASCII)
	uint8 meterData[LEN_METER_DATA];
	uint8 batt; // bit 6-7: meter battery, bit 0-5: terminal battery
	uint8 status;
	uint8 tempHour;
	uint8 tempData;
} PdaGroupDataReportOthers_t;

typedef struct {
	uint8 mtype;
	uint8 nodeBatt;
	uint8 mi;
	uint8 bt;
	uint8 ri;
	uint8 meterType;
	uint8 year[2];
	uint8 mon;
	uint8 day;
	uint8 hour;
	uint8 min;
	uint8 nData;
	uint8 meterData[LEN_METER_DATA];
	uint8 difVifBatt; // std    - upper nibble: 구경, lower nibble: 소숫점 자리수
		// others - bit 6-7: meter battery, bit 0-5: terminal battery
	uint8 meterStatus;
	uint8 diff[23][2];
} AmiMultiDataReport_t;

typedef struct {
	uint8 mtype;
	uint8 year[2];
	uint8 mon;
	uint8 day;
	uint8 hour;
	uint8 min;
	uint8 sec;
	uint8 mi;
	uint8 bt;
	uint8 ri;
	uint8 mode; // 단말기 동작 모드
	uint8 control; // 단말기 원격제어를 의한 필드 - unused
	uint8 protocolVer; // 프로토콜 버전 - unused
} AmiMultiDataAck_t;

//=========================================================
// Slave access finish macro interface for external access
//=========================================================
#define SLAVE_METERING_COMPLETE() APP_slaveMeteringCompleted()
#define SLAVE_METERING_FAIL(resultCode) APP_slaveMeteringFailed(resultCode)
#define SLAVE_ACCESS_COMPLETE() APP_slaveAccessCompleted()
#define SLAVE_ACCESS_FAIL() APP_slaveAccessFailed()

//=========================================================
// Slave access interface
//=========================================================
event32_t SLAVE_tasks(uint8 taskId, event32_t events);
SlaveAccessState_t SLAVE_getAccessState();
void SLAVE_init(uint8 taskId);
void SLAVE_runMessage(uint32 SrcAddr, byte *data, uint8 len, uint8 rssi);
BOOL SLAVE_isTimeSync();
BOOL SLAVE_needToUpdateSlaveInfo();
BOOL SLAVE_needToUpdateMeterInfo();
void SLAVE_getSlaveInfo(SlaveAccessInfo_t *pStatus);
BOOL SLAVE_updateMeterInfo();
BOOL SLAVE_updateSlaveInfo();
BOOL SLAVE_updateTimeSync();
BOOL SLAVE_metering(uint8 meteringType);
void SLAVE_close();
int SLAVE_longPreambleRequired(uint8 msgType);

// dummy functions
int SLAVE_reTxNeeded(uint8 msgType);
void SLAVE_activeStart(int sec);
void SLAVE_DataRequestCfm(uint32 destAddr, uint8 msgType, uint8 seq, BOOL result);
BOOL SLAVE_sendAckRequired(uint8 msgType, uint8 reqType);
#endif

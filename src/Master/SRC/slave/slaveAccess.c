#include "common_header.h"
#include "hal_spi_rf_trxeb.h"
#include "osal_Timer.h"
#include "meter.h"
#include "app.h"
#include "cc1200.h"
#include "slaveAccess.h"
#include "rtcAlarm.h"
#include "port_desc.h"
#include "uart.h"
#include "test.h"

#define SLAVE_REVERSE_FLOW_VALUE_MIN 65000

extern Config_t conf;

#if defined(DEBUG)
#define SLAVE_DBG(...)                                                                             \
	do {                                                                                       \
		if (TEST_isTestMode() == FALSE) {                                                  \
			printf_ts("[SLAVE]");                                                      \
			printf(__VA_ARGS__);                                                       \
		}                                                                                  \
	} while (0)
#else
#define SLAVE_DBG(...)                                                                             \
	do {                                                                                       \
		if (TEST_isTestMode() == FALSE) {                                                  \
			printf_ts(__VA_ARGS__);                                                    \
		}                                                                                  \
	} while (0);
#endif

static struct {
	BOOL isPowerOn;
	int taskId;
	int meteringType;
	int lastError;
	int meteringResult;
	uint8 lastRssi;
	Date_t lastAccessTime;
	SlaveAccessState_t state; // 현재 slave access state
	SlaveAccessState_t lastActiveState; // IDLe을 제외한 동작완료 된 state
} slaveAccessCtx;

static struct {
	uint8 serial[LEN_SLAVE_SERIAL_NUM + 1]; // slave device는 RF424장치이므로 일련번호가 8자리임.
	uint8 rssi;
	uint8 nodeType;
	uint8 timeSync;
	uint8 batt;
	uint8 mi;
	uint8 bt;
	uint8 ri;
	Date_t meterInfoUpdateTime;
} slaveDevice;

static void turnOn_cc1200()
{
	if (slaveAccessCtx.isPowerOn == 0) {
		SLAVE_DBG("CC1200 power ON\n");

		CC1200_rfOn();
		CC1200_setReady();
		slaveAccessCtx.isPowerOn = 1;
	}
}

static void turnOff_cc1200()
{
	if (slaveAccessCtx.isPowerOn) {
		SLAVE_DBG("CC1200 power OFF\n");
		// Power on을 최소 1번이라도 수행하여 set ready를 해야
		// SPI interface가 초기화 되므로 set sleep이 가능하다.
		// set ready를 안하고 set sleep을 할 경우 중간 과정에서
		// 무한 루프에 빠지게 된다. (driver level bug.)
		CC1200_setSleep();
	}
	CC1200_rfOff();

	slaveAccessCtx.isPowerOn = 0;
}

static void send_pdaGroupScanReq(int longPreamble, int retryCount)
{
	turnOn_cc1200();
	SLAVE_DBG("Send PDA_GROUP_SCAN_REQ (%s, retry:%d)\n", longPreamble ? "LONG" : "SHORT",
		  retryCount);

	uchar msg[0x100];
	memset(msg, 0, 0x100);

	PdaGroupScanReq_t *p = (PdaGroupScanReq_t *)msg;

	p->mtype = MSG_PDA_GROUP_SCAN_REQ;

	Date_t date;
	RTC_read(&date);

	memcpy(p->year, &date.year, 2);
	p->mon = date.mon;
	p->day = date.day;
	p->hour = date.hour;
	p->min = date.min;
	p->sec = date.sec;
	p->waitSec = 16; // TODO - Smart Phone App에서 설정된 시간에 맞추어야 함.

	uint8 ackFlag = retryCount > 1 ? NWK_HDR_FLAG_DATA_REQ : NWK_HDR_FLAG_DATA_RESP;
	CC1200_dataRequest(conf.slaveNwk, ackFlag, retryCount, msg, sizeof(PdaGroupScanReq_t));

	uint32 timeout = 13000;
	OSAL_startEventTimer(slaveAccessCtx.taskId, SLAVE_EVENT_SLAVE_ACCESS_TIMEOUT, timeout);
}

static void send_pdaMasterSlaveMeterReq(int longPreamble, int retryCount)
{
	turnOn_cc1200();
	SLAVE_DBG("Send PDA_MASTER_SLAVE_METER_REQ (%s, retry:%d)\n",
		  longPreamble ? "LONG" : "SHORT", retryCount);

	uchar msg[0x100];
	memset(msg, 0, 0x100);

	PdaMasterSlaveMeterReq_t *p = (PdaMasterSlaveMeterReq_t *)msg;
	p->mtype = MSG_PDA_MASTER_SLAVE_METER_REQ;

	uint8 ackFlag = retryCount > 1 ? NWK_HDR_FLAG_DATA_REQ : NWK_HDR_FLAG_DATA_RESP;
	CC1200_dataRequest(conf.slaveNwk, ackFlag, retryCount, msg,
			   sizeof(PdaMasterSlaveMeterReq_t));

	uint32 timeout = 3000 + retryCount * 3000;
	if (longPreamble) {
		timeout += 6000;
	}
	OSAL_startEventTimer(slaveAccessCtx.taskId, SLAVE_EVENT_SLAVE_ACCESS_TIMEOUT, timeout);
}

static void send_amiMasterSlaveCheckReq(int longPreamble, int retryCount)
{
	turnOn_cc1200();
	SLAVE_DBG("Send AMI_MASTER_SLAVE_CHECK_REQ (%s, retry:%d)\n",
		  longPreamble ? "LONG" : "SHORT", retryCount);

	uchar msg[0x100];
	memset(msg, 0, 0x100);

	AmiMasterSlaveCheckReq_t *p = (AmiMasterSlaveCheckReq_t *)msg;
	p->mtype = MSG_AMI_MASTER_SLAVE_CHECK_REQ;

	uint8 ackFlag = retryCount > 1 ? NWK_HDR_FLAG_DATA_REQ : NWK_HDR_FLAG_DATA_RESP;
	CC1200_dataRequest(conf.slaveNwk, ackFlag, retryCount, msg,
			   sizeof(AmiMasterSlaveCheckReq_t));

	uint32 timeout = 3000 + retryCount * 3000;
	if (longPreamble) {
		timeout += 6000;
	}

	OSAL_startEventTimer(slaveAccessCtx.taskId, SLAVE_EVENT_SLAVE_ACCESS_TIMEOUT, timeout);
}

static void send_amiMasterSlaveMeterReq(int longPreamble, int retryCount)
{
	turnOn_cc1200();
	SLAVE_DBG("Send AMI_MASTER_SLAVE_METER_REQ (%s, retry:%d)\n",
		  longPreamble ? "LONG" : "SHORT", retryCount);

	uchar msg[0x100];
	memset(msg, 0, 0x100);

	AmiMasterSlaveMeterReq_t *p = (AmiMasterSlaveMeterReq_t *)msg;
	p->mtype = MSG_AMI_MASTER_SLAVE_METER_REQ;

	Date_t date;
	RTC_read(&date);

	memcpy(p->year, &date.year, 2);
	p->mon = date.mon;
	p->day = date.day;
	p->hour = date.hour;
	p->min = date.min;
	p->sec = date.sec;

	uint8 ackFlag = retryCount > 1 ? NWK_HDR_FLAG_DATA_REQ : NWK_HDR_FLAG_DATA_RESP;
	CC1200_dataRequest(conf.slaveNwk, ackFlag, retryCount, msg,
			   sizeof(AmiMasterSlaveMeterReq_t));

	uint32 timeout = 3000 + retryCount * 3000;
	if (longPreamble) {
		timeout += 6000;
	}

	if (slaveAccessCtx.state == SLAVE_ACCESS_GET_DATA) {
		slaveAccessCtx.meteringResult = SLAVE_METERING_COMM_ERR;
	}
	OSAL_startEventTimer(slaveAccessCtx.taskId, SLAVE_EVENT_SLAVE_ACCESS_TIMEOUT, timeout);
}

static void send_amiMultiDataAck()
{
	turnOn_cc1200();
	SLAVE_DBG("Send AMI_MULTI_DATA_ACK\n");

	uchar msg[0x100];
	memset(msg, 0, 0x100);

	AmiMultiDataAck_t *p = (AmiMultiDataAck_t *)msg;
	p->mtype = MSG_AMI_MULTI_DATA_ACK;

	Date_t date;
	RTC_read(&date);

	memcpy(p->year, &date.year, 2);
	p->mon = date.mon;
	p->day = date.day;
	p->hour = date.hour;
	p->min = date.min;
	p->sec = date.sec;

	p->mi = slaveDevice.mi;
	p->bt = slaveDevice.bt;
	p->ri = slaveDevice.ri;

	uint8 ackFlag = NWK_HDR_FLAG_DATA_RESP; // 재전송 없게 하기 위함.
	CC1200_dataRequest(conf.slaveNwk, ackFlag, 1, msg, sizeof(AmiMultiDataAck_t));

	// timeout 불필요
}

static void close_slave()
{
	static uint8 wait_radio_tx_done_count = 0;

	if (CC1200_isRadioTxDone() == FALSE) {
		if (wait_radio_tx_done_count > 0) {
			SLAVE_DBG("Fail to wait send ACK to salve \n");
			wait_radio_tx_done_count = 0;
		} else {
			SLAVE_DBG("Wait send ACK to salve \n");
			wait_radio_tx_done_count++;
			OSAL_startEventTimer(slaveAccessCtx.taskId, SLAVE_EVENT_SLAVE_ACCESS_CLOSE,
					     3000);
			return;
		}
	}

	// Stop LCD metering wait icon
	// Have to working LCD redraw before slave complete
	LCD_redraw();

	// Turn off CC1200 before starting other tasks
	turnOff_cc1200();
	if (slaveAccessCtx.state == SLAVE_ACCESS_SET_TIME_SYNC ||
	    slaveAccessCtx.state == SLAVE_ACCESS_GET_SLAVE_INFO) {
		if (slaveAccessCtx.lastError == SLAVE_SUCCESS) {
			SLAVE_ACCESS_COMPLETE();
		} else {
			SLAVE_ACCESS_FAIL();
		}
	} else {
		if (slaveAccessCtx.lastError == SLAVE_SUCCESS) {
			SLAVE_METERING_COMPLETE();
		} else {
			SLAVE_METERING_FAIL(slaveAccessCtx.meteringResult);
		}
	}

	RTC_read(&slaveAccessCtx.lastAccessTime);
	slaveAccessCtx.lastActiveState = slaveAccessCtx.state;
	slaveAccessCtx.state = SLAVE_ACCESS_IDLE;
	slaveAccessCtx.meteringType = NOT_ACIVE_METERING;
	slaveAccessCtx.meteringResult = SLAVE_METERING_IDLE;
}

static void finish_slave_access(BOOL isSuccess)
{
	if (slaveAccessCtx.state == SLAVE_ACCESS_READ_METER) {
		// 즉시 검침시의 첫번째 단계 종료
		slaveAccessCtx.state = SLAVE_ACCESS_GET_DATA;
		slaveAccessCtx.meteringResult = SLAVE_METERING_COMM_ERR;
		send_pdaMasterSlaveMeterReq(SHORT_PREAMBLE, 3);
		return;
	}

	SLAVE_DBG("Slave access is finish, %s\n", (isSuccess) ? "SUCCESS" : "FAIL");

	slaveAccessCtx.lastError = (isSuccess) ? SLAVE_SUCCESS : SLAVE_FAIL;
	if (CC1200_isRadioTxDone()) {
		OSAL_startEventTimer(slaveAccessCtx.taskId, SLAVE_EVENT_SLAVE_ACCESS_CLOSE, 10);
	} else {
		// ACK를 위한 Tx가 남아있는 경우 이므로 delay 1초를 추가.
		OSAL_startEventTimer(slaveAccessCtx.taskId, SLAVE_EVENT_SLAVE_ACCESS_CLOSE, 1000);
	}
}

BOOL SLAVE_metering(uint8 meteringType)
{
	if (slaveAccessCtx.state != SLAVE_ACCESS_IDLE) {
		SLAVE_DBG("Slave is working (state=%d), cannot metering\n", slaveAccessCtx.state);
		return FALSE;
	}
	slaveAccessCtx.meteringResult = SLAVE_METERING_STARTED;

	SLAVE_DBG("Current metering type (%02x)\n", meteringType);
	slaveAccessCtx.meteringType = meteringType;
	slaveAccessCtx.lastError = SLAVE_NONE;

	BOOL ret = FALSE;
	switch (slaveAccessCtx.meteringType) {
	case INITIAL_METERING:
	case IMMEDIATE_METERING:
	case IMMEDIATE_METERING_FOR_REPORT:
		slaveAccessCtx.state = SLAVE_ACCESS_READ_METER;
		send_pdaGroupScanReq(LONG_PREAMBLE, 1);
		LCD_wait();
		ret = TRUE;
		break;

	case PERIODIC_METERING: {
		Date_t date;
		RTC_read(&date);

		int nHour = RTC_calcHourDiff(&slaveDevice.meterInfoUpdateTime, &date);
		SLAVE_DBG("Check meter info update cycle (%dh/%dh)\n", nHour,
			  INTERVAL_HOUR_AUTO_UPDATE_METER_INFO);
		if (nHour >= INTERVAL_HOUR_AUTO_UPDATE_METER_INFO) {
			// meter 정보를 갱신해야 할 필요가 있음
			slaveAccessCtx.state = SLAVE_ACCESS_GET_METER_INFO;
			send_pdaMasterSlaveMeterReq(LONG_PREAMBLE, 3);
		} else {
			slaveAccessCtx.state = SLAVE_ACCESS_GET_DATA;
			send_amiMasterSlaveMeterReq(LONG_PREAMBLE, 3);
		}
		LCD_wait(); // Display metering wait on LCD
		ret = TRUE;
	} break;

	default:
		SLAVE_DBG("Unknown or invalid metering type (%02x)\n", meteringType);
		break;
	}

	return ret;
}

void SLAVE_close()
{
	// 외부에서 RF424/433 동작 없이 바로 close를 수행할 경우
	turnOff_cc1200();
}

SlaveAccessState_t SLAVE_getAccessState()
{
	return slaveAccessCtx.state;
}

BOOL SLAVE_isTimeSync()
{
	return (slaveDevice.timeSync == 0) ? FALSE : TRUE;
}

void SLAVE_getSlaveInfo(SlaveAccessInfo_t *pStatus)
{
	memcpy(pStatus->slaveSerial, slaveDevice.serial, LEN_SLAVE_SERIAL_NUM);
	pStatus->slaveBatt = slaveDevice.batt;
	pStatus->masterRssi = slaveAccessCtx.lastRssi;
	pStatus->slaveRssi = slaveDevice.rssi;
	pStatus->lastError = slaveAccessCtx.lastError;

#if 0
    SLAVE_DBG("Serial Num   : %s \n", pStatus->slaveSerial);
    SLAVE_DBG("Battery      : %d00mV \n", pStatus->slaveBatt);
    SLAVE_DBG("Mater RSSI   : -%ddBm \n", pStatus->masterRssi);
    SLAVE_DBG("Slave RSSI   : -%ddBm \n", pStatus->slaveRssi);
    SLAVE_DBG("Last Error   : %d \n", pStatus->lastError);
#endif
}

BOOL SLAVE_needToUpdateSlaveInfo()
{
	if (slaveAccessCtx.lastActiveState == SLAVE_ACCESS_GET_SLAVE_INFO) {
		Date_t now;
		RTC_read(&now);
		int32 secDiff = RTC_calcSecDiff(&slaveAccessCtx.lastAccessTime, &now);
		if (secDiff < 0 || secDiff > INTERVAL_SEC_SLAVE_ACCESS) {
			return TRUE;
		} else {
			return FALSE;
		}
	} else {
		return TRUE;
	}
}

BOOL SLAVE_needToUpdateMeterInfo()
{
	if (slaveAccessCtx.lastActiveState == SLAVE_ACCESS_GET_METER_INFO) {
		Date_t now;
		RTC_read(&now);
		int32 secDiff = RTC_calcSecDiff(&slaveAccessCtx.lastAccessTime, &now);
		if (secDiff < 0 || secDiff > INTERVAL_SEC_SLAVE_ACCESS) {
			return TRUE;
		} else {
			return FALSE;
		}
	} else {
		return TRUE;
	}
}

BOOL SLAVE_updateMeterInfo()
{
	if (slaveAccessCtx.state != SLAVE_ACCESS_IDLE) {
		SLAVE_DBG("Slave is working (state=%d), cannot check slave\n",
			  slaveAccessCtx.state);
		return FALSE;
	}

	SLAVE_DBG("Slave meter info update\n");
	slaveAccessCtx.state = SLAVE_ACCESS_GET_METER_INFO;
	slaveAccessCtx.lastError = SLAVE_NONE;

	LCD_wait(); // Display metering wait on LCD
	OSAL_startEventTimer(slaveAccessCtx.taskId, SLAVE_EVENT_GET_METER_INFO, 10);

	return TRUE;
}

BOOL SLAVE_updateSlaveInfo()
{
	if (slaveAccessCtx.state != SLAVE_ACCESS_IDLE) {
		SLAVE_DBG("Slave is working (state=%d), cannot check slave\n",
			  slaveAccessCtx.state);
		return FALSE;
	}

	SLAVE_DBG("Slave status checking\n");
	slaveAccessCtx.state = SLAVE_ACCESS_GET_SLAVE_INFO;
	slaveAccessCtx.lastError = SLAVE_NONE;

	LCD_wait(); // Display metering wait on LCD
	OSAL_startEventTimer(slaveAccessCtx.taskId, SLAVE_EVENT_GET_SLAVE_INFO, 10);

	return TRUE;
}

BOOL SLAVE_updateTimeSync()
{
	if (slaveAccessCtx.state != SLAVE_ACCESS_IDLE) {
		SLAVE_DBG("Slave is working (state=%d), cannot time sync\n", slaveAccessCtx.state);
		return FALSE;
	}

	// slave의 시간 동기를 확인한 적이 없을 때만 전송.
	// 한번이라도 되었다면 주기검침으로 시간이 맞춰질 것이므로 이 절차 불필요
	if (slaveDevice.timeSync) {
		SLAVE_DBG("Slave already time synchronized\n");
		return FALSE;
	}

	SLAVE_DBG("Slave time synchronizing\n");
	slaveAccessCtx.state = SLAVE_ACCESS_SET_TIME_SYNC;
	slaveAccessCtx.lastError = SLAVE_NONE;

	LCD_wait(); // Display metering wait on LCD
	OSAL_startEventTimer(slaveAccessCtx.taskId, SLAVE_EVENT_SET_TIME_SYNC, 10);

	return TRUE;
}

void recv_amiMasterSlaveCheckReport(uint32 srcAddr, uint8 *pData, int len)
{
	SLAVE_DBG("Recv AMI_MASTER_SLAVE_CHECK_REPORT\n");

	AmiMasterSlaveCheckReport_t *p = (AmiMasterSlaveCheckReport_t *)pData;

	memcpy(slaveDevice.serial, p->serialNum, LEN_SLAVE_SERIAL_NUM);
	slaveDevice.batt = p->batt;
	slaveDevice.nodeType = p->nodeType;
	slaveDevice.rssi = p->rssi;
	slaveDevice.nodeType = p->nodeType;

	finish_slave_access(TRUE);
}

void recv_pdaGroupDataReport(uint32 srcAddr, uint8 *pData, int len)
{
	SLAVE_DBG("Recv PDA_GROUP_DATA_REPORT\n");

	// Meter S/N, Meter Type 추출

	uint8 meterSerial[4];
	uint8 caliberDp = 0;
	uint8 dif = 0;
	uint8 vif = 0;
	uint8 meterData[4];

	PdaGroupDataReportStd_t *pSTD = (PdaGroupDataReportStd_t *)pData;
	if (pSTD->moreFlag != 0) { // Metering Failed
		if (slaveAccessCtx.meteringType == PERIODIC_METERING) {
			// Slave가 long preamble로 access된 직후에는 10초 동안 대기 상태를 유지하므로
			// 여기는 short preamble을 사용해도 괜찮음.
			slaveAccessCtx.state = SLAVE_ACCESS_GET_DATA;
			send_amiMasterSlaveMeterReq(SHORT_PREAMBLE, 3);
		} else {
			slaveAccessCtx.meteringResult = SLAVE_METERING_MT_DOWN;
			finish_slave_access(FALSE);
		}
		return;
	}

	// 보조중계기의 metertype은 slave에서 보내준 것과 동일하게 해야 함.
	if (pSTD->meterType != conf.meterType) {
		conf.meterType = pSTD->meterType;
		FLASH_saveConfigInfo(&conf);
	}

	if (pSTD->meterType == W_STANDARD_D) {
		dif = pSTD->dif;
		vif = pSTD->vif;
		caliberDp = (pSTD->dif & 0xF0) | (pSTD->vif & 0x0F);
		memcpy(meterSerial, pSTD->meterSerial, 4);
		memcpy(meterData, pSTD->meterData, 4);
		SLAVE_DBG("Slave meter type STD (caliber/point : %02X)\n", caliberDp);
	} else { // 신한, M&S, Pulse
		PdaGroupDataReportOthers_t *pOthers = (PdaGroupDataReportOthers_t *)pData;

		if (pOthers->meterType == W_MNS_D) {
			caliberDp = 0x13; // MNS는 항상 15mm/소숫점 3자리
		} else if (pOthers->meterType == W_SHINHAN_D_BIG) {
			caliberDp = 0x12; // SH_B은 항상 15mm/소숫점 3자리
		} else {
			caliberDp = 0x13; // 그 외 기본 항상 15mm/소숫점 3자리
		}
		SLAVE_DBG("Slave meter type OTHERS(type : %d, caliber/point : %02X)\n",
			  pOthers->meterType, caliberDp);

		memcpy(meterSerial, pOthers->meterSerial, 4);
		memcpy(meterData, pOthers->meterData, 4);
	}

	// 계량기가 바뀌었는지 확인하고, 바뀌었으면 관련 정보 update
	// (단, 소수점/구경 정보가 invalid할 경우 생략.)
	uint8 dp = (caliberDp & 0x0F);
	uint8 caliber = (caliberDp & 0xF0) >> 4;
	if (dp < 7 && caliber > 0) { // 소수점 최대6자리, 구경값은 0이상 0xF이하인 경우 정상.
		METER_saveMeterInfo(meterSerial, caliberDp, dif, vif);
	}

	// 계량기 정보를 마지막으로 읽어 온 시간 기록
	RTC_read(&slaveDevice.meterInfoUpdateTime);

	// Temp 영역에 읽어온 데이터 저장
	MeterUnitData_t unit;
	memset(&unit, 0, sizeof(unit));

	Date_t date;
	RTC_read(&date);

	memcpy(unit.meterData, meterData, 4);

	METER_addTempData(&date, &unit);
	METER_displayTempData();

	if (slaveAccessCtx.meteringType == PERIODIC_METERING) {
		// Slave가 long preamble로 access된 직후에는 10초 동안 대기 상태를 유지하므로
		// 여기는 short preamble을 사용해도 괜찮음.
		slaveAccessCtx.state = SLAVE_ACCESS_GET_DATA;
		send_amiMasterSlaveMeterReq(SHORT_PREAMBLE, 3);
	} else {
		slaveAccessCtx.meteringResult = SLAVE_METERING_OK;
		finish_slave_access(TRUE);
	}
}

void recv_amiNodeEventAlarm(uint32 srcAddr, uint8 *pData, int len)
{
	// AMI_MASTER_SLAVE_METER_REQ(주기 검침)를 수신한 Slave는 다음과 같이 응답함
	// 1) 주기 검침 데이터가 있거나(정상), Slave의 검침 기록이 없음(MT_DOWN) - AMI_MULTI_DATA_REPORT로 응답
	// 2) Slave가 직전의 계량기 검침에 실패함(MT_DOWN) - AMI_NODE_EVENT_ALARM으로 응답

	// 그러나 AMI_MASTER_SLAVE_METER_REQ 메시지는 시간 동기용으로도 사용되므로 이 경우에는
	// Slave로부터 어떤 응답이든 받기만 하면 동기 성공임.

	SLAVE_DBG("Recv AMI_NODE_EVENT_ALARM (%02X)\n", *(pData + 1));

	if (slaveAccessCtx.state == SLAVE_ACCESS_GET_DATA) {
		SLAVE_DBG("Meter Down\n");
		slaveAccessCtx.meteringResult = SLAVE_METERING_MT_DOWN;
		finish_slave_access(FALSE);
	} else { // slaveAccessCtx.state = SLAVE_ACCESS_SET_TIME_SYNC;
		SLAVE_DBG("slave time sync\n");
		slaveDevice.timeSync = 1;
		finish_slave_access(TRUE);
	}
}

void recv_amiMultiDataReport(uint32 srcAddr, uint8 *pData, int len)
{
	SLAVE_DBG("Recv AMI_MULTI_DATA_REPORT\n");
	// slave 측에 ack 메시지를 전송함.
	send_amiMultiDataAck();

	if (slaveAccessCtx.state == SLAVE_ACCESS_SET_TIME_SYNC) {
		/**
         * AMI slave에 시간동기화 방법을 기존 "AMI S-M meter request"를 사용.
         * 이유 :
         *   "시간설정"은 경우 단순 시간 설정이며, 경우에 따라 동기화 안된 데이터가
         *   남아있을 수 있는 문제가 있음.
         *   "AMI S-M meter request"은 ACK 수신여부에 따라 기존 데이터를
         *   삭제하므로 해당 request를 사용.
         */
		SLAVE_DBG("Proc AMI_MULTI_DATA_REPORT - time sync\n");

		slaveDevice.timeSync = 1;
		finish_slave_access(TRUE);
	} else {
		// master/slave 양측 모두 시간 동기된 상태에서 받은 데이터만
		// 메시지에 포함된 시간이 정확하므로 사용 가능함.
		int valid = (RTC_isTimeSync() && slaveDevice.timeSync) ? 1 : 0;

		AmiMultiDataReport_t *p = (AmiMultiDataReport_t *)pData;

		// ack에 사용하기 위해 slave의 검침 파라미터를 저장해 둠
		slaveDevice.mi = p->mi;
		slaveDevice.bt = p->bt;
		slaveDevice.ri = p->ri;

		// slave device의 배터리 상태
		slaveDevice.batt = p->nodeBatt;

		MeterUnitData_t unit;
		memset(&unit, 0, sizeof(unit));

		unit.meterStatus = p->meterStatus;

		Date_t date;

		memcpy(&date.year, p->year, 2);
		date.mon = p->mon;
		date.day = p->day;
		date.hour = p->hour;
		date.min = p->min;
		date.sec = 0;

		memcpy(unit.meterData, p->meterData, 4);

		SLAVE_DBG(
			"Proc AMI_MULTI_DATA_REPORT - %d data, %d.%dV, mi:%d, ri:%d, status:%02x\n",
			p->nData, p->nodeBatt / 10, p->nodeBatt % 10, p->mi, p->ri,
			unit.meterStatus);

		// 검침데이터 4바이트가 모두 0xFF이면 검침불량
		int mtDown = 1;
		for (int i = 0; i < 4; i++) {
			if (unit.meterData[i] != 0xFF) {
				mtDown = 0;
				break;
			}
		}

		if (mtDown || p->nData == 0) {
			slaveAccessCtx.meteringResult = SLAVE_METERING_MT_DOWN;
			finish_slave_access(FALSE);
			return;
		}

		if (p->nData) {
			printf("    %2d) %04d-%02d-%02d %02d:%02d - %02x%02x%02x%02x\n", 1,
			       date.year, date.mon, date.day, date.hour, date.min,
			       unit.meterData[0], unit.meterData[1], unit.meterData[2],
			       unit.meterData[3]);

			if (valid) {
				METER_addStoredData(&date, &unit);
			}

			for (int i = 0; i < p->nData - 1; i++) {
				uint16 diff = 0;
				memcpy(&diff, p->diff[i], 2);
				if (diff >= SLAVE_REVERSE_FLOW_VALUE_MIN) {
					SLAVE_DBG("reverse flow diff, convert to '0'\n");
					diff = 0;
				}

				uint32 meterValue = 0;
				bcd2int(unit.meterData, &meterValue, 4);

				meterValue += diff;
				int2bcd(&meterValue, unit.meterData, 4);

				RTC_incDateHour(&date, p->mi);

				printf("    %2d) %04d-%02d-%02d %02d:%02d - %02x%02x%02x%02x - diff:%d\n",
				       i + 2, date.year, date.mon, date.day, date.hour, date.min,
				       unit.meterData[0], unit.meterData[1], unit.meterData[2],
				       unit.meterData[3], diff);

				if (valid) {
					METER_addStoredData(&date, &unit);
				}
			}

			METER_displayStoredData();
		}

		if (valid) {
			// Network로 보고 절차 개시
			slaveAccessCtx.meteringResult = SLAVE_METERING_OK;
			finish_slave_access(TRUE);
		} else {
			// 최초 부팅 시 시간동기화에 실패할 경우 발생.
			// 이 때 받은 주기 보고 데이터는 시간 동기 이전에 읽은 것이므로 Invalid 하므로
			// 해당 데이터를 무시하고 fail 처리한다.
			if (RTC_isTimeSync()) {
				SLAVE_DBG("Slave is not tmie sync - currently time sync\n");
				slaveDevice.timeSync = 1;
			} else {
				SLAVE_DBG("Slave is not tmie sync - cannot time sync\n");
			}
			finish_slave_access(FALSE);
		}
	}
}

void SLAVE_runMessage(uint32 SrcAddr, byte *data, uint8 len, uint8 rssi)
{
	// printRxMsg(SrcAddr, data, len);
	slaveAccessCtx.lastRssi = rssi;

	SLAVE_DBG("slave message received (mtype: %02X, len = %d)\n", len, *data);

	switch (data[0]) {
	case MSG_PDA_GROUP_DATA_REPORT:
		// PDA_MASTER_SLAVE_METER_REQ에 대한 응답 -  즉시 검침
		OSAL_stopEventTimer(slaveAccessCtx.taskId, SLAVE_EVENT_SLAVE_ACCESS_TIMEOUT);

		recv_pdaGroupDataReport(SrcAddr, data, len);
		break;

	case MSG_AMI_MULTI_DATA_REPORT:
		// AMI_MASTER_SLAVE_METER_REQ에 대한 응답 - 주기 검침: 데이터가 있거나 Slave의 검침 시도 전
		OSAL_stopEventTimer(slaveAccessCtx.taskId, SLAVE_EVENT_SLAVE_ACCESS_TIMEOUT);

		recv_amiMultiDataReport(SrcAddr, data, len);
		break;

	case MSG_AMI_MASTER_SLAVE_CHECK_REPORT:
		// AMI_MASTER_SLAVE_CHECK_REQ에 대한 응답 - 계량기 정보 update
		OSAL_stopEventTimer(slaveAccessCtx.taskId, SLAVE_EVENT_SLAVE_ACCESS_TIMEOUT);

		recv_amiMasterSlaveCheckReport(SrcAddr, data, len);
		break;

	case MSG_AMI_NODE_EVENT_ALARM:
		// AMI_MASTER_SLAVE_METER_REQ에 대한 응답 - 주기 검침: Slave의 검침 실패
		OSAL_stopEventTimer(slaveAccessCtx.taskId, SLAVE_EVENT_SLAVE_ACCESS_TIMEOUT);
		recv_amiNodeEventAlarm(SrcAddr, data, len);
		break;

	default:
		break;
	}
}

void SLAVE_init(uint8 taskId)
{
	slaveAccessCtx.taskId = taskId;

	memset(&slaveDevice, 0, sizeof(slaveDevice));
	memset(slaveDevice.serial, '0', 8); // 문자 '0'으로 채움
}

event32_t SLAVE_tasks(uint8 taskId, event32_t events)
{
	if (events & SLAVE_EVENT_SLAVE_ACCESS_TIMEOUT) {
		SLAVE_DBG("Task (Slave access timeout, state=%d)\n", slaveAccessCtx.state);
		finish_slave_access(FALSE);
		return (events ^ SLAVE_EVENT_SLAVE_ACCESS_TIMEOUT);
	}

	if (events & SLAVE_EVENT_SET_TIME_SYNC) {
		SLAVE_DBG("Task (Time sync, state=%d)\n", slaveAccessCtx.state);
		// AMI_MASTER_SLAVE_METER_REQ에 메시지에는 slave에 주는 시간 정보가 포함되어 있음
		// 주기 검침시마다 이 메시지를 보낼것이므로 이 때는 재전송없이 한 번만 보냄.
		send_amiMasterSlaveMeterReq(LONG_PREAMBLE, 2);
		return (events ^ SLAVE_EVENT_SET_TIME_SYNC);
	}

	if (events & SLAVE_EVENT_GET_SLAVE_INFO) {
		SLAVE_DBG("Task (Slave check, state=%d)\n", slaveAccessCtx.state);
		send_amiMasterSlaveCheckReq(LONG_PREAMBLE, 3);
		return (events ^ SLAVE_EVENT_GET_SLAVE_INFO);
	}

	if (events & SLAVE_EVENT_GET_METER_INFO) {
		SLAVE_DBG("Task (Meter info, state=%d)\n", slaveAccessCtx.state);
		send_pdaMasterSlaveMeterReq(LONG_PREAMBLE, 3);
		return (events ^ SLAVE_EVENT_GET_METER_INFO);
	}

	if (events & SLAVE_EVENT_SLAVE_ACCESS_CLOSE) {
		SLAVE_DBG("Task (Slave close, state=%d)\n", slaveAccessCtx.state);
		close_slave();
		return (events ^ SLAVE_EVENT_SLAVE_ACCESS_CLOSE);
	}

	return 0;
}

int SLAVE_longPreambleRequired(uint8 msgType)
{
	int longPreamble = 1;

	if (msgType == MSG_AMI_MULTI_DATA_ACK) {
		longPreamble = 0;
	}

	return longPreamble;
}

int SLAVE_reTxNeeded(uint8 msgType)
{
	return 1;
}
void SLAVE_activeStart(int sec)
{
}
void SLAVE_DataRequestCfm(uint32 destAddr, uint8 msgType, uint8 seq, BOOL result)
{
}
BOOL SLAVE_sendAckRequired(uint8 msgType, uint8 reqType)
{
	return FALSE;
}

#include <msp430.h>

#include "RTC.h"
#include "common_header.h"
#include "MSP430FlashUtil.h"

#include "osal_Timer.h"
#include "port_desc.h"
#include "app.h"
#include "check_meter_misc.h"
#include "battery.h"
#include "rtcAlarm.h"

#include "NFC_i2c.h"
#include "nfcProtocol.h"
#include "uart.h"
#include "modem.h"
#include "meter.h"
#include "dataFlash.h"

#if defined(AUX_REPEATER)
#include "slaveAccess.h"
#endif

#define NFC_RC_IN_METERING 0
#define NFC_RC_METERING_COMPLETED 1
#define NFC_RC_SERVER_ACCESS 2
#define NFC_RC_FAIL 3

#define CELLPHONE_NOMAL_RESPONSE 0

extern Config_t conf;
extern uint8 AppTaskId;
extern Metering_t Metering;

// NFC 프로토콜 v2를 위한 시간 구조체.
typedef struct {
	union {
		uint32 dateTime;
		struct {
			uint32 year : 6, mon : 4, day : 5, hour : 5, min : 6, sec : 6;
		};
	};
} NfcDateV2_t;

// NFC 비동기 동작 시 사용되는 Command buffer
struct {
	int len;
	uint8 cmd[64];
} nfcAsyncRx;

#if 1 // sholee
extern int internal_clock;
void enable_OFIE();

void disable_OFIE()
{
	SFRIE1 &= ~(0x0002);
}

void dev_clock_internal(void)
{
	// set watchdog register
	WDTCTL = WDTPW | WDTHOLD; // Stop watchdog timer

	// Set lowest possible DCOx, MODx
	UCSCTL0 = 0x00;

	// Select range for 12MHz operation
	UCSCTL1 = DCORSEL_6;

	// Set DCO frequency(maximum 20MHz) - Divider / (N + 1)
	UCSCTL2 = 249; // 8MHz

	// Set DCO FLL reference = REFO
	UCSCTL3 = SELREF_2;

	// Set MCLK = DCOCLK
	UCSCTL4 = SELS_3 + SELM_3;
}

void jumpToBSL()
{
	printf("start BSL ...\n");
	LCD_displayString("Call BSL");

	// disable meter t/rx port
	extern void set_meterPortForBSL();
	set_meterPortForBSL();

	TIMER_stop();

	disable_OFIE();
	dev_clock_internal();

	WDTCTL = WDTPW + WDTHOLD; // Stop watchdog timer
	SYSBSLC &= ~(SYSBSLPE + SYSBSLOFF);

	__disable_interrupt();

	// jump to BSL address
	((void (*)())0x1000)(); // jump to BSL

	printf("BSL started\n");
}

#endif

void NFCAPP_clearAsyncCmd()
{
	nfcAsyncRx.len = 0;
	memset(nfcAsyncRx.cmd, 0, 64);
}

static int copyToAsyncCmd(uint8 *p, int len)
{
	if (len < 10 || len >= 64 ||
	    (nfcAsyncRx.len == len && memcmp(nfcAsyncRx.cmd, p, len) == 0)) {
		return 0;
	}

	memcpy(nfcAsyncRx.cmd, p, len);
	nfcAsyncRx.len = len;

	return 1;
}

uint8 getMyDeviceCode()
{
#if LORA_DEVICE
	return DEVICE_CODE_LORA;
#else
	return DEVICE_CODE_NB;
#endif
}

void send(byte *p, int len)
{
	NfcHeader_t *head = (NfcHeader_t *)p;

	head->deviceCode = getMyDeviceCode();
#if LORA_DEVICE
	memcpy(head->device_id, conf.devEui, 8);
#else
	memcpy(head->device_id, conf.imei, 8);
#endif
	head->len = len - MSG_OFFSET_LEN;

	NFC_sendMessage(p, len);

	//yikim 2018.01.30
	NFC_checkRead(100); // 데이터 전송 후 스마트폰에서 읽어가는 이벤트 체크.

	//다음 메세지 대기
	OSAL_startEventTimer(AppTaskId, APP_EVENT_NFC_WAIT, (uint32)300);
	NFC_tagDetect();

	BATT_loadLastVoltage(); // 다음 보고시 사용할 목적으로 배터리 전압을 읽어 둠.
}

void NFCAPP_continueSend(byte *p, int len)
{
	NfcHeader_t *head = (NfcHeader_t *)p;

	head->deviceCode = getMyDeviceCode();
#if LORA_DEVICE
	memcpy(head->device_id, conf.devEui, 8);
#else
	memcpy(head->device_id, conf.imei, 8);
#endif
	head->len = len - MSG_OFFSET_LEN;

	NFC_sendMessage(p, len);
}

void setRtcV1(uint8 *p)
{
	Date_t date;

	date.year = *(p + 0) + *(p + 1) * 0x100;
	date.mon = *(p + 2);
	date.day = *(p + 3);
	date.hour = *(p + 4);
	date.min = *(p + 5);
	date.sec = *(p + 6);

	RTC_set(date);
}

void setRtcV2(uint8 *p)
{
	Date_t date;
	NfcDateV2_t tmp;

	memcpy((void *)&tmp.dateTime, p, sizeof(uint32));

	date.year = tmp.year + 2000;
	date.mon = tmp.mon;
	date.day = tmp.day;
	date.hour = tmp.hour;
	date.min = tmp.min;
	date.sec = tmp.sec;

	RTC_set(date);
}

void readRtcV1(uint8 *p)
{
	Date_t date;
	RTC_read(&date);

	*(p + 0) = date.year % 0x100;
	*(p + 1) = date.year / 0x100;
	*(p + 2) = date.mon;
	*(p + 3) = date.day;
	*(p + 4) = date.hour;
	*(p + 5) = date.min;
	*(p + 6) = date.sec;
}

void readRtcV2(uint8 *p)
{
	Date_t date;
	NfcDateV2_t tmp;

	RTC_read(&date);

	tmp.year = date.year - 2000;
	tmp.mon = date.mon;
	tmp.day = date.day;
	tmp.hour = date.hour;
	tmp.min = date.min;
	tmp.sec = date.sec;

	memcpy(p, (void *)&tmp.dateTime, sizeof(uint32));
}

///////////////////////////////
/////  Send Message
///////////////////////////////

void sendNodeConfReportV3(uint8 reportType, int error)
{
	byte msg[64];
	memset(msg, 0, sizeof(msg));

	NfcConfReportV1_t *p = (NfcConfReportV1_t *)msg;

#if LORA_DEVICE
	p->mtype = LORA_CONF_REPORT;
#else
	p->mtype = NB_CONF_REPORT;
#endif
#define NB_CONF_REPORT_V3 2 //단말기
	p->mversion = NB_CONF_REPORT_V3;
	p->reportType = reportType;
	if (p->reportType == REPORT_TYPE_SET) {
		if (error) {
			p->result = 1;
		} else {
			p->result = 0;
		}
		p->errorCode = error;
	}

	memcpy(p->serialNum, conf.serialNum, SERIAL_NUM_LEN);

	if (conf.sleepMode) {
		p->sleepMode = 1;
	} else {
		p->sleepMode = 2;
	}
#if LORA_DEVICE
	MODEM_copyAppEui(p->appEui);
#else
	MODEM_copyImsi(p->imsi);
	ip2hexArray(conf.serverIp, p->serverIp);

	uchar temp = 0;
	temp = p->serverIp[0];
	p->serverIp[0] = p->serverIp[3];
	p->serverIp[3] = temp;

	temp = p->serverIp[2];
	p->serverIp[2] = p->serverIp[1];
	p->serverIp[1] = temp;

	memcpy(p->serverPort, &conf.serverPort, 2);
#endif
	p->batt = BATT_getLastVoltage() & 0xff;
	memcpy(p->fwVer, FIRMWARE_VER, FIRMWARE_VER_LEN);
	p->meterInterval = conf.meterInterval;
	p->reportInterval = conf.reportInterval;

	readRtcV1(p->year);

#if 1
	p->nMeter = 1;
	p->meterInfo[0].meterPort = 1;
	p->meterInfo[0].meterType = conf.meterType;
#else
	p->nMeter = conf.meterNum;
	for (int i = 0; i < conf.meterNum; i++) {
		p->meterInfo[i].meterPort = conf.meterInfo[i].meterPort;
		p->meterInfo[i].meterType = conf.meterInfo[i].meterType;
	}
#endif

	// meter가 3개일 때를 기준으로 message format이 정의되어 있으므로 그 수가 모자라면 그만큼 줄임
	int msgLen = sizeof(NfcConfReportV1_t) - (3 - p->nMeter) * 2;

	send(msg, msgLen);
}

#if defined(AUX_REPEATER)

void sendNodeConfReportV4(uint8 reportType, int error)
{
	byte msg[64];
	memset(msg, 0, sizeof(msg));

	NfcConfReportV2_t *p = (NfcConfReportV2_t *)msg;

#if LORA_DEVICE
	p->mtype = LORA_CONF_REPORT;
#else
	p->mtype = NB_CONF_REPORT;
#endif

#define NB_CONF_REPORT_V4 3 // Repeater
	p->mversion = NB_CONF_REPORT_V4;
	p->reportType = reportType;
	if (p->reportType == REPORT_TYPE_SET) {
		if (error) {
			p->result = 1;
		} else {
			p->result = 0;
		}
		p->errorCode = error;
	}

	memcpy(p->serialNum, conf.serialNum, SERIAL_NUM_LEN);

	if (conf.sleepMode) {
		p->sleepMode = 1;
	} else {
		p->sleepMode = 2;
	}
#if LORA_DEVICE
	MODEM_copyAppEui(p->appEui);
#else
	MODEM_copyImsi(p->imsi);
	ip2hexArray(conf.serverIp, p->serverIp);

	uchar temp = 0;
	temp = p->serverIp[0];
	p->serverIp[0] = p->serverIp[3];
	p->serverIp[3] = temp;

	temp = p->serverIp[2];
	p->serverIp[2] = p->serverIp[1];
	p->serverIp[1] = temp;

	memcpy(p->serverPort, &conf.serverPort, 2);
#endif
	p->batt = BATT_getLastVoltage() & 0xff;
	memcpy(p->fwVer, FIRMWARE_VER, FIRMWARE_VER_LEN);
	p->meterInterval = conf.meterInterval;
	p->reportInterval = conf.reportInterval;

	readRtcV2(p->dateTime);

	memcpy(p->amiInfo.panId, &conf.pan_id, sizeof(uint16));
	memcpy(p->amiInfo.nwkAddr, &conf.nwk_addr, sizeof(uint32));

	int slaveIdPos = getFirstZeroPosition(conf.nwk_addr);
	uint8 *slaveNwk = (uint8 *)&conf.slaveNwk;
	p->amiInfo.slaveId = *(slaveNwk + slaveIdPos);

	send(msg, sizeof(NfcConfReportV2_t));
}

#endif // #if defined(AUX_REPEATER)

void sendNodeConfReport(uint8 reportType, int error)
{
#if defined(AUX_REPEATER)
	sendNodeConfReportV4(reportType, error);
#else
	sendNodeConfReportV3(reportType, error);
#endif
}

void sendMeterReport()
{
	byte msg[64];
	memset(msg, 0, sizeof(msg));

	NfcMeterReport_t *p = (NfcMeterReport_t *)msg;

	p->mtype = METER_REPORT;
	p->mversion = NFC_PROTOCOL_VER_1;

	p->batt = BATT_getLastVoltage() & 0xff;

	extern MeterTempData_t TempMeterData;
	int year = TempMeterData.unit.year + 2000;

	p->year[0] = year % 0x100;
	p->year[1] = year / 0x100;
	p->mon = TempMeterData.unit.mon;
	p->day = TempMeterData.unit.day;
	p->hour = TempMeterData.unit.hour;
	p->min = TempMeterData.unit.min;
	p->sec = TempMeterData.unit.sec;

	int needNewMetering = 0;

#if defined(AUX_REPEATER)
	SlaveAccessState_t state = SLAVE_getAccessState();
	if (state == SLAVE_ACCESS_IDLE) {
		if (SLAVE_needToUpdateMeterInfo()) {
			// Only check modem working. (not wait delay for MODEM)
			if (MODEM_getAccessState() == ACCESS_STATE_IN_PROGRESS) {
				p->meterState = NFC_RC_SERVER_ACCESS;
			} else {
				if (METER_needNewMetering(10) ==
				    0) { // 즉시 검침의 경우에는 10초 이하이면 재 검침하지 않음.
					p->meterState = NFC_RC_METERING_COMPLETED;
				} else {
					needNewMetering = 1;
					p->meterState = NFC_RC_IN_METERING; //0: 검침중 1: 검침완료
				}
			}
		} else {
			p->meterState = NFC_RC_METERING_COMPLETED;
		}
	} else if (state == SLAVE_ACCESS_GET_METER_INFO) {
		p->meterState = NFC_RC_IN_METERING;
	} else {
		// Another working, will be report BUSY
		p->meterState = NFC_RC_IN_METERING;
	}
#else // #if defined(AUX_REPEATER)
	if (Metering.inProcess) { // 현재 검침중이면 재 검침하지 않음
		p->meterState = NFC_RC_IN_METERING; //0: 검침중 1: 검침완료
	} else {
		if (METER_needNewMetering(10) ==
		    0) { // 즉시 검침의 경우에는 10초 이하이면 재 검침하지 않음.
			p->meterState = NFC_RC_METERING_COMPLETED;
		} else {
			needNewMetering = 1;
			p->meterState = NFC_RC_IN_METERING; //0: 검침중 1: 검침완료
		}
	}
#endif // #if defined(AUX_REPEATER)

	p->nMeter = 1;

	p->oneMeter[0].type = conf.meterType;
	p->oneMeter[0].port = 1;

#if defined(AUX_REPEATER)
	// TODO - M-S 간 통신불량 추가
	if (TempMeterData.noResponseFromSlave) {
		p->oneMeter[0].flag = 2; // 통신 불량
		TempMeterData.noResponseFromSlave = 0;
	} else
#endif
		if (TempMeterData.unit.meterData[0] == 0xff &&
		    TempMeterData.unit.meterData[1] == 0xff) {
		// previous metering failed
		p->oneMeter[0].flag = 1; // 검침불량
	} else {
		p->oneMeter[0].flag = 0; // 정상 검침
		memcpy(p->oneMeter[0].serial, TempMeterData.meterSerial, 4);
		memcpy(p->oneMeter[0].value, TempMeterData.unit.meterData, 4);
		if (conf.meterType == W_STANDARD_D) {
			p->oneMeter[0].status[0] = TempMeterData.caliberDp;
		} else {
			p->oneMeter[0].status[0] = 0;
		}
		p->oneMeter[0].status[1] = TempMeterData.unit.meterStatus;
	}

	int msgLen = sizeof(NfcMeterReport_t) - (3 - p->nMeter) * sizeof(NfcOneMeterReport_t);
	send(msg, msgLen);

	if (needNewMetering) {
		// 현재 검침중이 아니고 이전 검침으로부터 재검침이 필요한 시간만큼 흘렀으면 검침 시작
		OSAL_startEventTimer(AppTaskId, APP_EVENT_IMMEDIATE_METERING, (uint32)500);
	} else {
	}
}

void sendFwVerReport()
{
	byte msg[64];
	memset(msg, 0, sizeof(msg));

	NfcFwVerReport_t *p = (NfcFwVerReport_t *)msg;

	p->mtype = FW_VER_REPORT;
	p->mversion = NFC_PROTOCOL_VER_1;

	memcpy(p->fwVer, FIRMWARE_VER, FIRMWARE_VER_LEN);

	send(msg, sizeof(NfcFwVerReport_t));
}

void sendDeviceInfo()
{
	byte msg[64];
	memset(msg, 0, sizeof(msg));

	NfcDevInfoReport_t *p = (NfcDevInfoReport_t *)msg;

	p->mtype = DEVICE_INFO_REPORT;
	p->mversion = NFC_PROTOCOL_VER_1;

	memcpy(p->fwVer, FIRMWARE_VER, FIRMWARE_VER_LEN);
	memcpy(p->serialNum, conf.serialNum, SERIAL_NUM_LEN);
	p->deviceCode = getMyDeviceCode();

	p->deviceType = conf.termModel;

	send(msg, sizeof(NfcDevInfoReport_t));
}

void sendServerConnectReport(int reqType)
{
	byte msg[64];
	memset(msg, 0, sizeof(msg));

#if NBIOT_DEVICE
	NfcServerConnReportV2_t *p = (NfcServerConnReportV2_t *)msg;
	p->mtype = SERVER_CONNECT_REPORT;
	p->mversion = NFC_PROTOCOL_VER_2;
#else
	NfcServerConnReportV1_t *p = (NfcServerConnReportV1_t *)msg;
	p->mtype = SERVER_CONNECT_REPORT;
	p->mversion = NFC_PROTOCOL_VER_1;
#endif

	readRtcV1(p->currentYear);
	MODEM_copyLastAccessTime(p->lastAccessYear);

	p->serverAccessState = MODEM_getAccessState();
	p->accessStepOrResult = MODEM_getStateOrResult();
	p->rssi = MODEM_getLastRssi();
	p->snr = MODEM_getLastSNR();
#if NBIOT_DEVICE
	p->rsrp = MODEM_getLastRsrp();
	p->rsrq = MODEM_getLastRsrq();
#endif

#if NBIOT_DEVICE
	send(msg, sizeof(NfcServerConnReportV2_t));
#else
	send(msg, sizeof(NfcServerConnReportV1_t));
#endif

	if (reqType == 1) { // 접속 시작 명령
		if (p->serverAccessState == ACCESS_STATE_IDLE) {
			// 모뎀이 보고 중이면 새로운 보고를 지시하지 않음
			OSAL_stopEventTimer(AppTaskId, APP_EVENT_IMMEDIATE_REPORT);
			OSAL_startEventTimer(AppTaskId, APP_EVENT_IMMEDIATE_REPORT, (uint32)500);
			//            printf("server access started\n");
		} else {
			//            printf("server access requested ... but should wait few second\n");
		}
	} else {
		//        printf("server access NOT requested\n");
	}
}

void sendPulseMeterValueAck()
{
	byte msg[64];
	memset(msg, 0, sizeof(msg));

	NfcPulseMeterValueSetAck_t *p = (NfcPulseMeterValueSetAck_t *)msg;

	p->mtype = PULSE_METER_VALUE_ACK;
	p->mversion = NFC_PROTOCOL_VER_1;

	send(msg, sizeof(NfcPulseMeterValueSetAck_t));
}

void sendBdControlAck(uint8 mversion, uint8 reset)
{
	byte msg[64];
	memset(msg, 0, sizeof(msg));

	// Protocol version 관계없이 동일한 Data 먼저 처리.
	NfcBdCtrlAckV1_t *p = (NfcBdCtrlAckV1_t *)msg;

	p->mtype = BD_CONTROL_ACK;
	p->mversion = mversion;
	p->reset = reset;

	if (conf.sleepMode) {
		p->sleepMode = SLEEP_MODE_SLEEP;
	} else {
		p->sleepMode = SLEEP_MODE_NORMAL;
	}

	// Protocol version에 따라 다른 부분 처리.
	int messageLen = sizeof(NfcBdCtrlAckV1_t);

	if (mversion >= NFC_PROTOCOL_VER_2) {
		messageLen = sizeof(NfcBdCtrlAckV2_t);

		NfcBdCtrlAckV2_t *p = (NfcBdCtrlAckV2_t *)msg;

		if (conf.riCtrlMode) {
			p->reportMode = REPORT_MODE_TERMINAL;
		} else {
			p->reportMode = REPORT_MODE_SERVER;
		}
	}

	if (mversion >= NFC_PROTOCOL_VER_3) {
		messageLen = sizeof(NfcBdCtrlAckV3_t);

		NfcBdCtrlAckV3_t *p = (NfcBdCtrlAckV3_t *)msg;

		if (conf.periodMode) {
			p->periodMode = PERIOD_MODE_USE;
		} else {
			p->periodMode = PERIOD_MODE_NO_USE;
		}
	}

	if (mversion >= NFC_PROTOCOL_VER_4) {
		messageLen = sizeof(NfcBdCtrlAckV4_t);

		NfcBdCtrlAckV4_t *p = (NfcBdCtrlAckV4_t *)msg;

		if (conf.debugPrint == 2) {
			p->debugMode = DEBUG_METER_MODE_USE;
		} else if (conf.debugPrint == 1) {
			p->debugMode = DEBUG_JATG_MODE_USE;
		} else {
			p->debugMode = DEBUG_MODE_NO_USE;
		}
	}

	send(msg, messageLen);
}

#if defined(AUX_REPEATER)
void sendSlaveCheckReport()
{
	byte msg[64];
	memset(msg, 0, sizeof(msg));

	NfcSlaveCheckReport_t *p = (NfcSlaveCheckReport_t *)msg;

	p->mtype = SLAVE_CHECK_REPORT;
	p->mversion = NFC_PROTOCOL_VER_1;

	memcpy(p->nwkAddr, (void *)&conf.slaveNwk, sizeof(uint32));

	SlaveAccessState_t state = SLAVE_getAccessState();
	if (state == SLAVE_ACCESS_IDLE) {
		if (SLAVE_needToUpdateSlaveInfo()) {
			// Only check modem working. (not wait delay for MODEM)
			if (MODEM_getAccessState() == ACCESS_STATE_IN_PROGRESS) {
				p->checkState = NFC_RC_SERVER_ACCESS;
			} else {
				SLAVE_updateSlaveInfo();
				p->checkState = NFC_RC_IN_METERING;
			}
		} else {
			// 정해진 Interval내 받은 Slave info가 있을 경우 해당 데이터 전송.
			SlaveAccessInfo_t pStatus;
			memset(&pStatus, 0, sizeof(SlaveAccessInfo_t));

			SLAVE_getSlaveInfo(&pStatus);

			p->checkState = (pStatus.lastError == SLAVE_SUCCESS) ?
						NFC_RC_METERING_COMPLETED :
						NFC_RC_FAIL;
			memcpy(p->slaveSerialNum, pStatus.slaveSerial, LEN_SLAVE_SERIAL_NUM);
			p->slaveBatt = pStatus.slaveBatt;
			p->slaveRssi = pStatus.slaveRssi;
			p->masterRssi = pStatus.masterRssi;
		}
	} else if (state == SLAVE_ACCESS_GET_SLAVE_INFO) {
		p->checkState = NFC_RC_IN_METERING;
	} else {
		p->checkState = NFC_RC_SERVER_ACCESS;
	}

	send(msg, sizeof(NfcSlaveCheckReport_t));
}
#endif // #if defined(AUX_REPEATER)

///////////////////////////////
/////  Recv Message
///////////////////////////////

// void Recv_Node_Conf_Req(byte* data, uint8 len)
// {
//     sendNodeConfReport(REPORT_TYPE_REQ, 0);
// }

void recvNodeConfSetV3(byte *data, uint8 len)
{
	NfcConfSetV3_t *p = (NfcConfSetV3_t *)data;

/* #define NB_CONF_SET_V1 0 // 단말기 U316 이후 사용 안함*/
/* #define NB_CONF_SET_V2 1 // Repeater */
#define NB_CONF_SET_V3 2 // Service Code 추가
	printf_ts("protocol ver : %d\n", p->mversion);
	setRtcV1(p->year);

	Config_t tconfig;
	memcpy(&tconfig, &conf, sizeof(Config_t));

	int error = 0;
	int changed = 0;
	do {
		// p->serialNum --> check하여 에러 처리할 필요가 있는가?
		if (memcmp(p->serialNum, tconfig.serialNum, SERIAL_NUM_LEN) != 0) {
			error = CONFIG_ERR_SERIAL_NUM;
			break;
		}

		if (p->sleepMode == 1) {
			if (tconfig.sleepMode == 0) {
				tconfig.sleepMode = 1;
				changed = 1;
			}
		} else if (p->sleepMode == 2) {
			if (tconfig.sleepMode) {
				tconfig.sleepMode = 0;
				changed = 1;
			}
		}

		uint8 mi = p->meterInterval;
		uint8 ri = p->reportInterval;

		if ((24 % mi) != 0 || (24 % ri) != 0 || mi > ri) {
			error = CONFIG_ERR_METERING_PARAM;
			break;
		}

		if (tconfig.meterInterval != mi) {
			tconfig.meterInterval = mi;
			changed = 1;
		}

		if (tconfig.reportInterval != ri) {
			tconfig.reportInterval = ri;
			changed = 1;
		}

#if NBIOT_DEVICE
		char needChangeCode = FALSE;

		for (int i = 0; i < 4; i++) {
			if (p->serviceCode[i] != 0) {
				needChangeCode = TRUE;
			}
		}

		if (needChangeCode) {
			if (memcmp(tconfig.serviceCode, p->serviceCode, 4) != 0) {
				memcpy(tconfig.serviceCode, p->serviceCode, 4);
				// 설정 변경 시 Global context에 복사 후 이벤트 등록
				memcpy(&conf, &tconfig, sizeof(Config_t));
				changed = 1;
			}
		}

		char newIP[SERVER_IP_STR_LEN];
		sprintf(newIP, "%d.%d.%d.%d", p->serverIp[3], p->serverIp[2], p->serverIp[1],
			p->serverIp[0]);

		if (strncmp(newIP, tconfig.serverIp, SERVER_IP_STR_LEN) != 0) {
			strcpy(tconfig.serverIp, newIP);
			changed = 1;
		}

		uint16 newPort;
		memcpy(&newPort, p->serverPort, 2);

		if (newPort != tconfig.serverPort) {
			tconfig.serverPort = newPort;
			changed = 1;
		}
#endif
		uint8 mt = p->meterInfo[0].meterType;
		if (mt != W_STANDARD_D && mt != W_SHINHAN_D && mt != W_SHINHAN_D_BIG &&
		    mt != W_MNS_D) {
			error = CONFIG_ERR_METER_TYPE;
			break;
		}

		if (tconfig.meterType != mt) {
			tconfig.meterType = mt;
			changed = 1;
		}

		// TODO : Check meter port (ref. Need to implementation dynamic UART port assign)
		// error =  CONFIG_ERR_METER_PORT;

		error = 0;
	} while (0);

	if (error == 0 && changed) {
		// error가 없고 파라미터가 바뀐 경우에만 저장함.
		memcpy(&conf, &tconfig, sizeof(Config_t));
		OSAL_setEvent(AppTaskId, APP_EVENT_CHANGE_CONFIG);
	}

	sendNodeConfReport(REPORT_TYPE_SET, error);
}

#if defined(AUX_REPEATER)

void recvNodeConfSetV4(byte *data, uint8 len)
{
	NfcConfSetV4_t *p = (NfcConfSetV4_t *)data;

	setRtcV2(p->dateTime);

	Config_t tconfig;
	memcpy(&tconfig, &conf, sizeof(Config_t));

	int error = 0;
	int changed = 0;
	do {
		// p->serialNum --> check하여 에러 처리할 필요가 있는가?
		if (memcmp(p->serialNum, tconfig.serialNum, SERIAL_NUM_LEN) != 0) {
			error = CONFIG_ERR_SERIAL_NUM;
			break;
		}

		if (p->sleepMode == 1) {
			if (tconfig.sleepMode == 0) {
				tconfig.sleepMode = 1;
				changed = 1;
			}
		} else if (p->sleepMode == 2) {
			if (tconfig.sleepMode) {
				tconfig.sleepMode = 0;
				changed = 1;
			}
		}

		uint8 mi = p->meterInterval;
		uint8 ri = p->reportInterval;

		if ((24 % mi) != 0 || (24 % ri) != 0 || mi > ri) {
			error = CONFIG_ERR_METERING_PARAM;
			break;
		}

		if (tconfig.meterInterval != mi) {
			tconfig.meterInterval = mi;
			changed = 1;
		}

		if (tconfig.reportInterval != ri) {
			tconfig.reportInterval = ri;
			changed = 1;
		}

#if NBIOT_DEVICE
		char needChangeCode = FALSE;

		for (int i = 0; i < 4; i++) {
			if (p->serviceCode[i] != 0) {
				needChangeCode = TRUE;
			}
		}

		if (needChangeCode) {
			if (memcmp(tconfig.serviceCode, p->serviceCode, 4) != 0) {
				memcpy(tconfig.serviceCode, p->serviceCode, 4);
				// 설정 변경 시 Global context에 복사 후 이벤트 등록
				memcpy(&conf, &tconfig, sizeof(Config_t));
				changed = 1;
			}
		}

		char newIP[SERVER_IP_STR_LEN];
		sprintf(newIP, "%d.%d.%d.%d", p->serverIp[3], p->serverIp[2], p->serverIp[1],
			p->serverIp[0]);

		if (strncmp(newIP, tconfig.serverIp, SERVER_IP_STR_LEN) != 0) {
			strcpy(tconfig.serverIp, newIP);
			changed = 1;
		}

		uint16 newPort;
		memcpy(&newPort, p->serverPort, 2);

		if (newPort != tconfig.serverPort) {
			tconfig.serverPort = newPort;
			changed = 1;
		}
#endif
		uint16 panId;
		uint32 nwkAddr;
		memcpy((void *)&panId, p->amiInfo.panId, sizeof(uint16));
		memcpy((void *)&nwkAddr, p->amiInfo.nwkAddr, sizeof(uint32));

		if (tconfig.pan_id != panId) {
			tconfig.pan_id = panId;
			changed = 1;
		}

		if (tconfig.nwk_addr != nwkAddr) {
			tconfig.nwk_addr = nwkAddr;
			// Master NWK변경 시 Slave도 변경되야 하므로 새로운 NWK로 변경.
			tconfig.slaveNwk = tconfig.nwk_addr;
			changed = 1;
		}

		int slaveIdPos = getFirstZeroPosition(tconfig.nwk_addr);
		uint8 *slaveNwk = (uint8 *)&tconfig.slaveNwk;
		if (*(slaveNwk + slaveIdPos) != p->amiInfo.slaveId) {
			*(slaveNwk + slaveIdPos) = p->amiInfo.slaveId;
			changed = 1;
		}

		error = 0;
	} while (0);

	if (error == 0 && changed) {
		// error가 없고 파라미터가 바뀐 경우에만 저장함.
		memcpy(&conf, &tconfig, sizeof(Config_t));
		OSAL_setEvent(AppTaskId, APP_EVENT_CHANGE_CONFIG);
	}

	sendNodeConfReport(REPORT_TYPE_SET, error);
}

#endif // #if defined(AUX_REPEATER)

void recvNodeConfSet(byte *data, uint8 len)
{
#if defined(AUX_REPEATER)
	recvNodeConfSetV4(data, len);
#else
	recvNodeConfSetV3(data, len);
#endif
}

void recvMeterReq(byte *data, uint8 len)
{
	NfcMeterReq_t *p = (NfcMeterReq_t *)data;

	setRtcV1(p->year);
	sendMeterReport();
}

void sendFwUpdateResp(int result, int state)
{
	byte msg[64];
	memset(msg, 0, sizeof(msg));

	NfcFwUpdateResp_t *p = (NfcFwUpdateResp_t *)msg;
	p->mtype = FW_UPDATE_REPORT;
	p->mversion = NFC_PROTOCOL_VER_1;
	p->result = result;
	p->state = state;
	memcpy(p->serialNum, conf.serialNum, SERIAL_NUM_LEN);
	memcpy(p->fwVer, FIRMWARE_VER, FIRMWARE_VER_LEN);

	send(msg, sizeof(NfcFwUpdateResp_t));
}

void recvFwUpdateReq(byte *data, uint8 len)
{
	NfcFwUpdateReq_t *p = (NfcFwUpdateReq_t *)data;

	int result = 0;

	if (p->mversion == NFC_PROTOCOL_VER_1) {
#define REQ_STATUS 0
#define READY_BSL 1

		if (p->reqMode == REQ_STATUS) {
			if (!conf.bslModel) {
#define NOT_BSL_BOARD 3
				result = NOT_BSL_BOARD;
			} else {
				if (memcmp(p->serialNum, conf.serialNum, SERIAL_NUM_LEN) != 0) {
#define SERIAL_NOT_MATCH 1
					result = SERIAL_NOT_MATCH;
				} else if (MODEM_getAccessState() != ACCESS_STATE_IDLE) {
#define RUNNING_COMM 2
					result = RUNNING_COMM;
				}
			}

			sendFwUpdateResp(result, p->reqMode);
		} else if (p->reqMode == READY_BSL) {
			if (conf.bslModel) {
				result = CELLPHONE_NOMAL_RESPONSE;
				sendFwUpdateResp(result, p->reqMode);

				OSAL_startEventTimer(AppTaskId, APP_EVENT_FW_UPDATE, (uint32)1500);
			}
		}
	}
}

void recvPeriodMeterReq(byte *data, uint8 len)
{
	NfcPeriodMeterReq_t *pReq = (NfcPeriodMeterReq_t *)data;

	uint16 yearFrom = ((pReq->from_year[1] << 8) | pReq->from_year[0]) - 2000;
	uint16 monFrom = pReq->from_mon;
	uint16 dayFrom = pReq->from_day;

	uint16 yearTo = ((pReq->to_year[1] << 8) | pReq->to_year[0]) - 2000;
	uint16 monTo = pReq->to_mon;
	uint16 dayTo = pReq->to_day;

	dataFlash_sendData((uint8)yearFrom, monFrom, dayFrom, (uint8)yearTo, monTo, dayTo);
}

void recvFlashDateListReq(byte *data, uint8 len)
{
	byte msg[64];
	memset(msg, 0, sizeof(msg));

	NfcFlashDateListReport_t *p = (NfcFlashDateListReport_t *)&msg;
	p->mtype = FLASH_DATE_LIST_REPORT;
	p->mversion = 0;

	if (MODEM_getAccessState() != ACCESS_STATE_IDLE) {
		p->resultState = 1;
	} else {
		p->nMonth = dataFlash_getList(p->dayList);
		if (p->nMonth > 5) {
			p->nMonth = 5;
		}
	}

	int msgLen = sizeof(NfcFlashDateListReport_t) - ((5 - p->nMonth) * 6);
	send(msg, msgLen);
}

void recvFlashDataReq(byte *data, uint8 len)
{
	NfcFlashDataReq_t *p = (NfcFlashDataReq_t *)data;

	byte msg[64];
	memset(msg, 0, sizeof(msg));

	NfcFlashDataReport_t *pMsg = (NfcFlashDataReport_t *)msg;

	pMsg->mtype = FLASH_DATA_REPORT;
	pMsg->mversion = 0;

	if (MODEM_getAccessState() != ACCESS_STATE_IDLE) {
		pMsg->resultState = 1;
		send(msg, sizeof(NfcFlashDataReport_t));
	} else {
		int result = dataFlash_sendData(p->yearFrom, p->monFrom, p->dayFrom, p->yearTo,
						p->monTo, p->dayTo);

		if (result == 0) {
			pMsg->nTotal = 0;
			pMsg->nCurrent = 0;
			send(msg, sizeof(NfcFlashDataReport_t));
		}
	}
}

void recvServerConnectReq(byte *data, uint8 len)
{
	NfcServerConnReq_t *p = (NfcServerConnReq_t *)data;
	sendServerConnectReport(p->reqType);
}

void recvPulseMeterValueSet(byte *data, uint8 len)
{
	//NfcPulseMeterValueSet_t *p = (NfcPulseMeterValueSet_t *)data;
	sendPulseMeterValueAck();
}

void recvBdControlReq(byte *data, uint8 len)
{
	NfcBdCtrlReqV1_t *p = (NfcBdCtrlReqV1_t *)data;

	int sleepModeChanged = 0;
	int reportModeChanged = 0;
	int periodModeChanged = 0;
	int debugModeChanged = 0;

	Config_t tconfig;
	memcpy(&tconfig, &conf, sizeof(Config_t));

	// Protocol version 관계없는 공통부분 설정 확인.
	if (p->sleepMode == SLEEP_MODE_SLEEP) {
		if (tconfig.sleepMode == 0) {
			tconfig.sleepMode = 1;
			sleepModeChanged = 1;
		}
	} else if (p->sleepMode == SLEEP_MODE_NORMAL) {
		if (tconfig.sleepMode) {
			tconfig.sleepMode = 0;
			sleepModeChanged = 1;
		}
	}

	int mVersion = p->mversion;

	// Protocol version 2에 대한 설정 확인.
	if (mVersion >= NFC_PROTOCOL_VER_2) {
		NfcBdCtrlReqV2_t *pMsg = (NfcBdCtrlReqV2_t *)data;
		if (pMsg->reportMode == REPORT_MODE_SERVER) {
			if (tconfig.riCtrlMode) {
				tconfig.riCtrlMode = 0;
				tconfig.riCtrlChgCount = 0;
				tconfig.riCtrlValue = tconfig.reportInterval;
				reportModeChanged = 1;
			}
		} else if (pMsg->reportMode == REPORT_MODE_TERMINAL) {
			if (tconfig.riCtrlMode == 0) {
				tconfig.riCtrlMode = 1;
				tconfig.riCtrlChgCount = 0;
				tconfig.riCtrlValue = tconfig.reportInterval;
				reportModeChanged = 1;
			}
		}
	}

	// Protocol version 3에 대한 설정 확인.
	if (mVersion >= NFC_PROTOCOL_VER_3) {
		NfcBdCtrlReqV3_t *pMsg = (NfcBdCtrlReqV3_t *)data;

		if (pMsg->periodMode == PERIOD_MODE_USE) {
			tconfig.periodMode = 1;
			periodModeChanged = 1;
		} else if (pMsg->periodMode == PERIOD_MODE_NO_USE) {
			tconfig.periodMode = 0;
			periodModeChanged = 1;
		}
	}

	if (mVersion >= NFC_PROTOCOL_VER_4) {
		NfcBdCtrlReqV4_t *pMsg = (NfcBdCtrlReqV4_t *)data;

		if (pMsg->debugMode == DEBUG_METER_MODE_USE) {
			conf.debugPrint = 2;
			PRINT_resume();
			UART_debugMode();
		} else if (pMsg->debugMode == DEBUG_JATG_MODE_USE) {
			conf.debugPrint = 1;
			UART_init();
			PRINT_resume();
			PRINT_enable();
		} else if (pMsg->debugMode == DEBUG_MODE_NO_USE) {
			tconfig.debugPrint = 0;
			debugModeChanged = 1;
		}
	}

	if (sleepModeChanged || reportModeChanged || periodModeChanged || debugModeChanged) {
		// 설정 변경 시 Global context에 복사 후 이벤트 등록
		memcpy(&conf, &tconfig, sizeof(Config_t));
		OSAL_setEvent(AppTaskId, APP_EVENT_CHANGE_CONFIG);
	} else {
		if (p->reset) {
			OSAL_setEvent(AppTaskId, APP_EVENT_REBOOT);
		}
	}

	sendBdControlAck(mVersion, p->reset);
}

///////////////////////////////
/////  Send Recv SMW Message
///////////////////////////////

void sendMeterAdjustError()
{
	byte msg[64];
	memset(msg, 0, sizeof(msg));

	NfcMeterAdjustError_t *p = (NfcMeterAdjustError_t *)msg;

	p->mtype = METER_ADJUST_ERROR;
	p->mversion = 0;

	memcpy(p->serialNum, conf.serialNum, SERIAL_NUM_LEN);
	memcpy(p->fwVer, FIRMWARE_VER, FIRMWARE_VER_LEN);

	p->result = 1; // SN mismatch

	send(msg, sizeof(NfcMeterAdjustError_t));
}

uint32 reqTime = 0;

void NFCAPP_meterAdjustReq()
{
	reqTime = TIMER_getMsec();
	//    PRINT_hexBuffer(nfcAsyncRx.cmd, nfcAsyncRx.len);

	NfcMeterAdjustReq_t *p = (NfcMeterAdjustReq_t *)nfcAsyncRx.cmd;

	if (p->whatWithSN != IGNORE_SERIAL_NUM) {
		if (memcmp(conf.serialNum, p->serialNum, SERIAL_NUM_LEN) != 0) {
			if (p->whatWithSN == CHANGE_SERIAL_NUM) {
				memcpy(conf.serialNum, p->serialNum, SERIAL_NUM_LEN);
				FLASH_saveConfigInfo(&conf);
			} else {
				sendMeterAdjustError();
				return;
			}
		}
	}

	// 미터 조정을 위한 요청이라는 걸 기록하고, 재전송은 없음
	Metering.forMeterAdjust = 1;
	Metering.nRetry = 1;
	Metering.meterType = conf.meterType;

	METER_bypassReq(p->body, p->bodyLen);

	// nfc로 돌려 줄 회신을 meter로부터 수신할 때까지 다소 시간이 걸림
	//  그 시간 동안 vcc를 계속 유지하기 위해 timeout을 길게 설정
	OSAL_stopEventTimer(AppTaskId, APP_EVENT_NFC_WAIT);
	OSAL_startEventTimer(AppTaskId, APP_EVENT_NFC_WAIT, (uint32)1500);

	OSAL_startEventTimer(AppTaskId, APP_EVENT_METER_TIMEOUT, (uint32)1200);
}

void NFCAPP_meterAdjustResp(uint8 *body, int bodyLen)
{
	if (SMART_WATER_METER(conf.termModel) == 0) {
		// 스마트미터가 아니면 무시
		return;
	}

	OSAL_stopEventTimer(AppTaskId, APP_EVENT_NFC_WAIT);
	OSAL_startEventTimer(AppTaskId, APP_EVENT_NFC_WAIT, (uint32)700);

	byte msg[64];
	memset(msg, 0, sizeof(msg));

	NfcMeterAdjustResp_t *p = (NfcMeterAdjustResp_t *)msg;

	p->mtype = METER_ADJUST_RESP;
	p->mversion = 0;

	memcpy(p->serialNum, conf.serialNum, SERIAL_NUM_LEN);
	memcpy(p->fwVer, FIRMWARE_VER, FIRMWARE_VER_LEN);

	if (bodyLen >= 35) {
		bodyLen = 35;
	}

	p->bodyLen = bodyLen;
	memcpy(p->body, body, bodyLen);

	send(msg, sizeof(NfcMeterAdjustResp_t) - 35 + bodyLen);

	//    PRINT_hexBuffer(p->body, p->bodyLen);
	//    PRINT_hexBuffer(msg, sizeof(NfcMeterAdjustResp_t)-35+bodyLen);

	uint32 msDiff = TIMER_getMsec() - reqTime;
	//    printf("[%ld ms] ", msDiff);
}

void recvMeterAdjustRequest(byte *data, uint8 len)
{
	if (SMART_WATER_METER(conf.termModel) == 0) {
		// 스마트미터가 아니면 무시
		return;
	}

	if (copyToAsyncCmd(data, len) == 0) {
		return;
	}

	OSAL_setEvent(AppTaskId, APP_EVENT_METER_ADJUST);
}

///////////////////////////////
/////  Send Recv NB-IoT for U+ Message
///////////////////////////////
#if (defined(NBIOT_LG_TYPE) || defined(NBIOT_BASE_TYPE))

void sendNbiotIdReport(uint8 reportType)
{
	byte msg[64];
	memset(msg, 0, sizeof(msg));

	NfcNbiotIdReport_t *p = (NfcNbiotIdReport_t *)msg;

	memcpy(p->serviceCode, conf.serviceCode, 4);
	p->mtype = NB_ID_REPORT;
#define NB_ID_REPORT_V2 1
	p->mversion = NB_ID_REPORT_V2;
	p->reportType = reportType;

	MODEM_copyCseId(p->cseid);
	MODEM_copyIccid(p->iccid);

	send(msg, sizeof(NfcNbiotIdReport_t));
}

void recvNbiotIdSet(byte *data, uint8 len)
{
	NfcNbiotIdSet_t *p = (NfcNbiotIdSet_t *)data;

	Config_t tconfig;

	char needChangeCode = FALSE;

	for (int i = 0; i < 4; i++) {
		if (p->serviceCode[i] != 0) {
			needChangeCode = TRUE;
		}
	}

	if (needChangeCode) {
		memcpy(&tconfig, &conf, sizeof(Config_t));
		if (memcmp(tconfig.serviceCode, p->serviceCode, 4) != 0) {
			memcpy(tconfig.serviceCode, p->serviceCode, 4);
			// 설정 변경 시 Global context에 복사 후 이벤트 등록
			memcpy(&conf, &tconfig, sizeof(Config_t));
			OSAL_setEvent(AppTaskId, APP_EVENT_CHANGE_CONFIG);
		}
	}

	sendNbiotIdReport(REPORT_TYPE_SET);
}

#endif

#if LORA_DEVICE

void sendLoRaAppEuiKeyReport(uint8 reportType, int result, int error)
{
	byte msg[64];
	memset(msg, 0, sizeof(msg));

	NfcLoRaAppEuiKeyReport_t *p = (NfcLoRaAppEuiKeyReport_t *)msg;

	p->mtype = LORA_APP_EUI_KEY_REPORT;
	p->mversion = 0;
	p->reportType = reportType;
	p->result = result;
	p->errorCode = error;
	if (result == NFC_RC_METERING_COMPLETED && error == CONFIG_ERR_NONE) {
		// 정상완료인 경우에만 설정
		MODEM_copyAppEui(p->appEui);
		MODEM_copyAppKey(p->appKey);
	}

	memcpy(p->fwVer, FIRMWARE_VER, FIRMWARE_VER_LEN);
	memcpy(p->serialNum, conf.serialNum, SERIAL_NUM_LEN);

	send(msg, sizeof(NfcLoRaAppEuiKeyReport_t));
}

void recvLoRaAppEuiKeySet(byte *data, uint8 len)
{
	NfcLoRaAppEuiKeySet_t *p = (NfcLoRaAppEuiKeySet_t *)data;

	int result, error;
	if (memcmp(p->serialNum, conf.serialNum, SERIAL_NUM_LEN) != 0) {
		//  Serial number 불일치 애러
		result = NFC_RC_FAIL;
		error = CONFIG_ERR_SERIAL_NUM;
	} else {
		if (MODEM_getAccessState() == ACCESS_STATE_IN_SETTING) {
			// 모뎀 설정 중
			result = NFC_RC_IN_METERING;
			error = CONFIG_ERR_NONE;
		} else {
			JoinMode_t joinMode = (p->joinMode == JOIN_MODE_SEUDO_JOIN) ?
						      LORA_SEUDO_JOIN :
						      LORA_REAL_JOIN;
			if (MODEM_setUserNwk(p->appEui, p->appKey, joinMode)) {
				// 현재 AppKey or AppEUI 이 불일치 할 경우 TRUE로 설정 시작.
				result = NFC_RC_IN_METERING;
				error = CONFIG_ERR_NONE;
			} else {
				// 모뎀 AppKey and AppEUI 이 요청받은 값과 동일할 경우 완료 및 보고.
				result = NFC_RC_METERING_COMPLETED;
				error = CONFIG_ERR_NONE;
			}
		}
	}

	sendLoRaAppEuiKeyReport(REPORT_TYPE_SET, result, error);
}

#endif

///////////////////////////////
/////  NFC process and print
///////////////////////////////

// void printNfcMsg(int dir, byte *msg, uint8 len)
// {
// #if 1
//     printf("\n[%s] MSG_TYPE(%02X) (%02X)",
//         dir?"TX":"RX", *(msg + 10), *(msg + 10));

//     char *p = (char *)(msg+10);

//     printf("\n");

//     for(int i = 0; i < len-10; i++) {
//         if(!(i%0x16)) {
//             printf("\n    ");
//         }
//         printf("%02x ", *(p + i));
//     }
//     printf("\n");
// #endif
// }

void NFCAPP_runMessage(byte *data, uint8 len)
{
	switch (data[MSG_TYPE_POSITION]) {
	case NODE_CONF_REQ:
		// Recv_Node_Conf_Req(data, len);
		sendNodeConfReport(REPORT_TYPE_REQ, CONFIG_ERR_NONE);
		break;

	case LORA_CONF_SET:
	case NB_CONF_SET:
		recvNodeConfSet(data, len);
		break;

	case FW_UPDATE_REQ:
		recvFwUpdateReq(data, len);
		break;

	case METER_REQ:
		recvMeterReq(data, len);
		break;

	case SERVER_CONNECT_REQ:
		recvServerConnectReq(data, len);
		break;

	case BD_CONTROL_REQ:
		recvBdControlReq(data, len);
		break;

	// 메인 플래쉬 메모리를 사용하는 버전에서 이전 기간검침 요청 메시지가
	// 수신되면 날자 목록을 요청한 것과 동일하게 처리하기로 함.
	case FLASH_DATE_LIST_REQ:
	case PERIOD_METER_REQ:
		recvFlashDateListReq(data, len);
		break;

	case FLASH_DATA_REQ:
		recvFlashDataReq(data, len);
		break;

	case PULSE_METER_VALUE_SET:
		// Not support this command
		sendFwVerReport();
		break;

	case DEVICE_INFO_REQ:
		sendDeviceInfo();
		break;

	case SLAVE_CHECK_REQ:
#if defined(AUX_REPEATER)
		sendSlaveCheckReport();
#else
		sendFwVerReport();
#endif
		break;

#if (defined(NBIOT_LG_TYPE) || defined(NBIOT_BASE_TYPE))
	case NB_ID_REQ:
		sendNbiotIdReport(REPORT_TYPE_REQ);
		break;

	case NB_ID_SET:
		recvNbiotIdSet(data, len);
		break;
#else
	case NB_ID_REQ:
	case NB_ID_SET:
		sendFwVerReport();
		break;
#endif

#if LORA_DEVICE
	case LORA_APP_EUI_KEY_REQ:
		sendLoRaAppEuiKeyReport(REPORT_TYPE_REQ, NFC_RC_METERING_COMPLETED,
					CONFIG_ERR_NONE);
		break;

	case LORA_APP_EUI_KEY_SET:
		recvLoRaAppEuiKeySet(data, len);
		break;
#else
	case LORA_APP_EUI_KEY_REQ:
	case LORA_APP_EUI_KEY_SET:
		sendFwVerReport();
		break;
#endif
	case METER_ADJUST_REQ:
		recvMeterAdjustRequest(data, len);
		break;

	default:
		sendFwVerReport();
		break;
	}
}

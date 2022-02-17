#ifndef TDD_TEST
#include <msp430.h>
#include <ctype.h>

#include "RTC.h"
#include "common_header.h"
#include "MSP430FlashUtil.h"

#include "port_desc.h"
#include "check_meter_misc.h"
#include "uart.h"
#include "lcdDriver.h"
#include "battery.h"
#include "NFC_i2c.h"

#include "osal_Timer.h"
#include "Task_Mgr.h"

#include "app.h"
#include "flashDriver.h"
#include "meter.h"
#include "rtcAlarm.h"
#include "test.h"
#include "modem.h"
#include "nfcProtocol.h"
#include "dataFlash.h"

#if defined(AUX_REPEATER)
#include "slaveAccess.h"
#include "cc1200.h"
#endif

uint8 AppTaskId;
uint8 AppProcess = APP_IDLE;
Config_t conf;
Metering_t Metering;

void APP_showConfig(Config_t *p)
{
	char serial[SERIAL_NUM_LEN + 1] = "";
	memcpy(serial, p->serialNum, SERIAL_NUM_LEN);
	serial[SERIAL_NUM_LEN] = '\0';

	printf("===========================================\n");
#if LORA_DEVICE
	printf("         LoRa Terminal(Solu-M)\n");
#else // NBIOT_DEVICE
	printf("         NB-IoT Terminal\n");
#endif
	printf("===========================================\n");
	printf(" [ Model      ] %s \n", TERM_MODEL_STRING(p->termModel));
	printf(" [ Build      ] %s %s (%s)\n", __DATE__, __TIME__, FIRMWARE_VER);
	printf(" [ S/N        ] %s\n", serial);
#if LORA_DEVICE
	printf(" [ Device EUI ] %02x%02x%02x-%02x%02x-%02x%02x%02x\n", p->devEui[0], p->devEui[1],
	       p->devEui[2], p->devEui[3], p->devEui[4], p->devEui[5], p->devEui[6], p->devEui[7]);
#else // NBIOT_DEVICE
	printf(" [ IMEI       ] %02x-%02x%02x%02x-%02x%02x%02x-%x\n", p->imei[0], p->imei[1],
	       p->imei[2], p->imei[3], p->imei[4], p->imei[5], p->imei[6], p->imei[7] / 0x10);
#if defined(NBIOT_LG_TYPE)
	printf(" [ Server     ] %s : %d, %s\n", p->serverIp, p->serverPort, p->serviceCode);
#else
	printf(" [ Server     ] %s : %d, %s\n", p->serverIp, p->serverPort, p->serviceCode);
	printf(" [ FOTA       ] %s : %d, %dday\n", p->fotaIp, p->fotaPort, p->fotaInterval);
#endif
#endif
	if (p->isShortInterval) {
		printf(" [ MI & RI    ] %d (minutes)\n", p->reportInterval);
		printf(" [ Base Min   ] %d min\n", p->reportMin);
	} else {
		printf(" [ MI/RI/SI   ] %d/%d/%d (hours)\n", p->meterInterval, p->reportInterval,
		       METER_DATA_SAVE_INTERVAL);
		printf(" [ Report Hour] ");
		if (p->reportInterval == 1) {
			printf("Every Hour\n");
		} else {
			for (int h = 0; h < 24; h++) {
				if (APP_checkTimeInterval(h, p->intervalBaseTime,
							  p->reportInterval)) {
					printf("%d, ", h);
				}
			}
			printf("\n");
		}
		printf(" [ Report Min ] %d (%d sec)\n", p->reportMin, p->reportSec);
	}
	printf(" [ RI Control ] %s \n", (p->riCtrlMode > 0) ? "On" : "Off");
	printf(" [ PeriodMode ] %s \n", (p->periodMode > 0) ? "On" : "Off");
	printf(" [ Meter Type ] %s\n", METER_getMeterName(p->meterType));
	printf(" [ Operating  ] %s\n", p->sleepMode ? "Sleep" : "Active");
	uint8 batt = BATT_getLastVoltage();
	printf(" [ Battery    ] %d.%dV\n", batt / 10, batt % 10);
	printf(" [ Reset Count] %d times\n", p->resetCount);
#define DEBUG_OFF 0
#define DEBUG_JTAG 1
#define DEBUG_METER 2
#define DEBUG_STATUS_STR(x)                                                                        \
	(((x) == DEBUG_OFF) ?                                                                      \
		 "OFF" :                                                                           \
		 ((x) == DEBUG_JTAG) ? "JTAG" : ((x) == DEBUG_METER) ? "METER" : "UNKNOWN")
	printf(" [ Debug Out  ] %s(%s)\n", p->debugPrint ? "On" : "Off",
	       DEBUG_STATUS_STR(p->debugPrint));
	printf(" [ Dataskip   ] %d, (0:24hour), (1:4day)\n", p->dataSkipMode);
#if defined(AUX_REPEATER)
	printf(" [ PAN ID     ] %04X\n", p->pan_id);

	uint8 *master = (uint8 *)&p->nwk_addr;
	uint8 *slave = (uint8 *)&p->slaveNwk;
	printf(" [ NWK        ] %d.%d.%d.%d\n", *(master + 3), *(master + 2), *(master + 1),
	       *(master + 0));
	printf(" [ Slave NWK  ] %d.%d.%d.%d\n", *(slave + 3), *(slave + 2), *(slave + 1),
	       *(slave + 0));
	printf(" [ Button     ] %d\n", conf.havePushButton);
#endif
	printf(" [ RST CAUSE  ] %Xh \n", SYSRSTIV);
	printf("===========================================\n");
	printf("\n");
}

void REBOOT_SYSTEM()
{
	MISC_delayMs(100);

	GLOBAL_DISABLE_INT();
	SYSTEM_RESET();
	MISC_delayMs(100);
}

//===========================================
//INTERRUPT Service Routine
//===========================================

// RTC interrupt service routine
#pragma vector = RTC_VECTOR
__interrupt void RTC_ISR(void)
{
	if (RTCIV & RTCAIFG) {
		RTCIV &= ~(RTCAIFG);
		WAKEUP_DEVICE();
		OSAL_setEvent(AppTaskId, APP_EVENT_RTC_ALARM);
	}
}

// Port 1 interrupt service routine - NFC
#pragma vector = PORT1_VECTOR
__interrupt void Port_1(void)
{
	if (PORT1_IFG & BM(PORT_NFC_TAG)) {
		PORT1_IFG &= ~BM(PORT_NFC_TAG);
		NFC_tagDetect();
		OSAL_startEventTimer(AppTaskId, APP_EVENT_NFC_WAIT, (uint32)700);
		WAKEUP_DEVICE();

		NFCAPP_clearAsyncCmd();
	}

	if (PORT1_IFG & BM(PORT_NFC_FD_IN)) {
		PORT1_IFG &= ~BM(PORT_NFC_FD_IN);
		NFC_recvMessage();
		OSAL_startEventTimer(AppTaskId, APP_EVENT_NFC_WAIT, (uint32)700);
	}

#if defined(AUX_REPEATER)
	if (PORT1_IFG & BM(PORT_LCD_SWITCH)) {
		PORT1_IFG &= ~BM(PORT_LCD_SWITCH);
		if (conf.havePushButton) {
			OSAL_stopEventTimer(AppTaskId, APP_EVENT_SENSOR_REED);
			if (TEST_isTestMode() == TRUE) {
				OSAL_startEventTimer(AppTaskId, APP_EVENT_SENSOR_REED, (uint32)10);
			} else {
				OSAL_startEventTimer(AppTaskId, APP_EVENT_SENSOR_REED, (uint32)500);
			}
			WAKEUP_DEVICE();
		}
	}
#endif
}

// Port 2 interrupt service routine - Reed, Tamper, Flood
#pragma vector = PORT2_VECTOR
__interrupt void Port_2(void)
{
	if (PORT_SENSOR_IFG & BM(PORT_LCD_SWITCH)) {
		// lora 단말기에는 push button switch가 달려있지 않음
		PORT_SENSOR_IFG &= ~BM(PORT_LCD_SWITCH);
	}

	if (PORT_SENSOR_IFG & BM(PORT_SENSOR_REED)) {
		PORT_SENSOR_IFG &= ~BM(PORT_SENSOR_REED); // P1.2 IFG cleared
		OSAL_stopEventTimer(AppTaskId, APP_EVENT_SENSOR_REED);
		if (TEST_isTestMode() == TRUE) {
			OSAL_startEventTimer(AppTaskId, APP_EVENT_SENSOR_REED, (uint32)10);
		} else {
			OSAL_startEventTimer(AppTaskId, APP_EVENT_SENSOR_REED, (uint32)500);
		}
		WAKEUP_DEVICE();
	}

#if defined(AUX_REPEATER)
	// CC1200 tx/rx interrupt
	if (PORT_SENSOR_IFG & BM(PORT_CC1200_GPIO0)) {
		PORT_SENSOR_IFG &= ~BM(PORT_CC1200_GPIO0);

		if (SLAVE_getAccessState() == SLAVE_ACCESS_IDLE &&
		    CC1200_getRfMode() == RF_MODE_RX) {
			// Rx 동작은 Slave operating이 아닌 경우 쓰레기 값으로 간주하고 그 외에는 모두 처리.
			// (Slave에 대한 ACK 전송 처리가 Interrupt에 의한 비동기로 처리되므로
			//  Slave operating이 아니더라도 ISR에 의한 처리가 필요.)
			if (TEST_isTestMode() == FALSE) {
				// Test mode인 경우 모두 처리.
				return;
			}
		}

		if (CC1200_getRfMode() == RF_MODE_TX) {
			//PORT_SENSOR_IES &= ~BM(PORT_CC1200_GPIO0);   // P2.7 Low/Hi edge
			CC1200_isr();
			WAKEUP_DEVICE();
		} else { // RF_MODE_RX
			if (PORT_SENSOR_IES & BM(PORT_CC1200_GPIO0)) {
				PORT_SENSOR_IES &= ~BM(PORT_CC1200_GPIO0); // P2.7 Low/Hi edge
				CC1200_isr();
				WAKEUP_DEVICE();
				CC1200_worDuty(WOR_SHORT_DUTY);
				CC1200_wor();
			} else { // Change Len
				PORT_SENSOR_IES |= BM(PORT_CC1200_GPIO0); // P2.7 Hi/Low edge
				CC1200_read_rxByte();
				WAKEUP_DEVICE();
			}
			SLAVE_activeStart(10);
		}
	}
#endif
}

#pragma vector = UNMI_VECTOR
__interrupt void UNMI_ISR(void)
{
	if (SFRIFG1 & OFIFG) { // OSC fault interrupt flag
		SFRIFG1 &= ~OFIFG; // Clear OSC Fault flag
		UCSCTL7 &= ~(XT1LFOFFG + DCOFFG); // Clear XT1 & DCO fault flags
		SYSTEM_RESET();
	}

	if (SFRIFG1 & NMIIFG) { // NMI pin interrupt flag
		SFRIFG1 &= ~NMIIFG;
		SYSTEM_RESET();
	}

	if (SFRIFG1 & VMAIFG) { // Vacant memory access interrupt flag
		SFRIFG1 &= ~VMAIFG;
		SYSTEM_RESET();
	}
}

#if !defined(AUX_REPEATER)
static void ctrlReportInterval()
{
#define RI_CTRL_MAX_THRESHOLD 3 // 최종 반영 횟수
#if LORA_DEVICE
#define RI_CTRL_DEC_SIG_QUAL 90 // 보고주기 감소 기준 값
#define RI_CTRL_INC_SIG_QUAL 120 // 보고주기 증가 기준 값
#else
#define RI_CTRL_DEC_SIG_QUAL 105 // 보고주기 감소 기준 값
#define RI_CTRL_INC_SIG_QUAL 110 // 보고주기 증가 기준 값
#endif

	if (!conf.riCtrlMode) {
		printf_ts("RI CTRL : OFF\n");
		return;
	}

	BOOL isIncCtrl = (conf.riCtrlValue > conf.reportInterval) ? TRUE : FALSE;
	int error = MODEM_getStateOrResult();
	switch (error) {
#if LORA_DEVICE
	// LoRa의 경우 GET TIME 실패도 데이터 전송은 성공한 것이므로 통신성공으로 간주한다.
	case ERROR_CODE_GET_TIME:
#endif
	case ERROR_CODE_NONE: {
#if LORA_DEVICE
		int sigQual = MODEM_getLastRssi();
#else
		int sigQual = MODEM_getLastRsrp();
#endif
		if (sigQual <= RI_CTRL_DEC_SIG_QUAL) {
			conf.riCtrlChgCount = (isIncCtrl) ? (conf.riCtrlChgCount + 1) : 0;
		} else if (sigQual >= RI_CTRL_INC_SIG_QUAL) {
			conf.riCtrlChgCount = (isIncCtrl) ? 0 : (conf.riCtrlChgCount + 1);
		} else {
			conf.riCtrlChgCount = 0;
		}
	} break;
#if LORA_DEVICE
	// LoRa의 경우 Join 실패도 통신실패로 간주한다.
	case ERROR_CODE_NOT_JOINED:
#endif
	case ERROR_CODE_COMM: {
		conf.riCtrlChgCount = (isIncCtrl) ? 0 : (conf.riCtrlChgCount + 1);
	} break;

	default:
		// conf.riCtrlChgCount 값 유지.
		break;
	}

	if (conf.riCtrlChgCount >= RI_CTRL_MAX_THRESHOLD) {
		conf.riCtrlChgCount = 0;
		if (isIncCtrl) {
			conf.riCtrlValue = conf.reportInterval;
		} else {
			conf.riCtrlValue = conf.reportInterval * 2;
			if (conf.riCtrlValue > 24)
				conf.riCtrlValue = 24;
		}
	}

	printf_ts("RI CTRL : On, Current RI(%d) Threshold(%d/%d) \n", conf.riCtrlValue,
		  conf.riCtrlChgCount, RI_CTRL_MAX_THRESHOLD);
}
#endif

void APP_prepareToSleep()
{
#if defined(AUX_REPEATER)
	SLAVE_close();
#endif

	MODEM_close();
	AppProcess = APP_IDLE;

	if (conf.sleepMode) {
		LCD_displayString("SLEEP");
	} else {
		LCD_redraw();
		if (Metering.success == 0) {
#if defined(AUX_REPEATER)
			LCD_displayError(Metering.slaveMeteringError);
#else
			LCD_displayError(ERROR_METERING);
#endif
		}
	}
	RTC_alarmSchedule();
}

void APP_init(uint8 taskId)
{
	AppTaskId = taskId;

	PRINT_enable();

	FLASH_readConfigInfo(&conf);
	if (conf.debugPrint > 1) {
		conf.debugPrint = 0;
	}

	conf.riCtrlValue = conf.reportInterval;
	conf.riCtrlChgCount = 0;

	BATT_init();
	LCD_init();
	NFC_init();
	RTC_init();

	FLASH_updateResetCount(&conf);
	APP_showConfig(&conf);

	if (conf.debugPrint == 0) {
		printf("\n--console disabled--\n");
		MISC_delayMs(2000);
		PRINT_stop();
		PRINT_disable();
	}
	// Flash memory에 저장된 데이터 점검
#if !defined(AUX_REPEATER)
	dataFlash_init();
#endif

	MODEM_initialization();

	// 초기화 직후 LCD에 펌웨어 버전을 표시
	LCD_clear();
	LCD_displayFirmwareVersion();
	MISC_delayMs(500);

	if (conf.sleepMode) {
		// NFC통신 후 Sleep여부에 따라 H마크를 표시히자만,
		// H/W Reset이 발생할 경우 H마크가 초기화되기 때문에
		// Sleep 모드인 경우에만 H마크 On동작을 수행한다.
		if (SMART_WATER_METER(conf.termModel)) {
			MISC_delayMs(1500); // 검침기 MCU 부팅 및 통신 준비시간 대기
			METER_std_lcdMarkReq(conf.sleepMode);
			MISC_delayMs(100);
		}
		APP_prepareToSleep();
		return;
	}

	METER_deleteAllData();
	LCD_redraw();

	AppProcess = APP_IDLE;
	OSAL_setEvent(AppTaskId, APP_EVENT_INITIAL);
}

static void meteringCompleted()
{
	if (conf.isShortInterval == 1 && Metering.meteringType == PERIODIC_METERING) {
		MODEM_open(APP_PERIODIC_REPORT);
		return;
	}

	if (AppProcess != APP_IDLE) {
		// APP process가 IDLE이 아니더라도 초기 보고 절차는 무조건 수행.
		if (Metering.meteringType == INITIAL_METERING) {
			MODEM_close();
		} else {
			printf_ts("App process is not IDLE\n");
			return;
		}
	}

	if (Metering.meteringType == INITIAL_METERING) {
		MODEM_open(APP_INITIAL_REPORT);
	} else if (Metering.meteringType == IMMEDIATE_METERING_FOR_REPORT) {
		MODEM_open(APP_IMMEDIATE_REPORT);
	}
#if defined(AUX_REPEATER)
	// 보조중계기로 쓰일때에는 slave 주기 검침 직후 report.
	else if (Metering.meteringType == PERIODIC_METERING) {
		MODEM_open(APP_PERIODIC_REPORT);
	}
#endif
	else {
		APP_prepareToSleep();
	}
}

#if defined(AUX_REPEATER)

void APP_slaveAccessCompleted()
{
	printf_ts("Slave access complete\n");
	// Metering.inProcess = 0;
	// Metering.success = 1;
	APP_prepareToSleep();
}

void APP_slaveAccessFailed()
{
	printf_ts("Slave access fail\n");
	// Metering.inProcess = 0;
	// Metering.success = 0;
	APP_prepareToSleep();
}

void APP_slaveMeteringCompleted()
{
	printf_ts("Slave metering OK\n");
	Metering.inProcess = 0;
	Metering.success = 1;
	Metering.slaveMeteringError = ERROR_NONE;

	// Slave 검침 후 LCD에 검침데이터를 표시할 때 LCD Icon 정보가 없으므로
	// 기본 Icon 정보를 설정한 후 LCD에 표시함.
	if (Metering.meteringType == PERIODIC_METERING) {
		// 최초 주기동작에서 Slave가 검침데이터를 안줄 수 있음.
		if (METER_getNumberOfStoredData() > 0) {
			MeterUnitData_t *unit = METER_getLastData();
			unit->icon.fArrow = 1;
			unit->icon.m3 = 1;
			unit->icon.notUsed = 0;
			LCD_updateMeterValue((uint8 *)unit); // Display meter data
		}
	} else {
		MeterUnitData_t *unit = METER_getTempData();
		unit->icon.fArrow = 1;
		unit->icon.m3 = 1;
		unit->icon.notUsed = 0;
		LCD_updateMeterValue((uint8 *)unit); // Display meter data
	}

	meteringCompleted();
}

void APP_slaveMeteringFailed(int resultCode)
{
	printf_ts("Slave metering (%d) failed\n", Metering.meteringType);

	if (resultCode == SLAVE_METERING_MT_DOWN) {
		printf_ts("Meter Down\n");
		Metering.slaveMeteringError = ERROR_METERING;
	} else {
		printf_ts("No response from slave device\n");
		Metering.slaveMeteringError = ERROR_REPORT;
	}

	Metering.inProcess = 0;
	Metering.success = 0;

	MeterUnitData_t unit;
	memset(&unit, 0, sizeof(unit));
	memset(unit.meterData, 0xff, 4);
	unit.meterStatus = 0xff;

	if (Metering.meteringType == PERIODIC_METERING) {
		if (resultCode == SLAVE_METERING_MT_DOWN) {
			void meteringUpdateData(BOOL isSuccess, MeterUnitData_t * unit);
			meteringUpdateData(FALSE, &unit);
		} else {
			// 통신 불량이 발생하면 그 동안 저장되어 있던 데이터를 모두 지움
			// 서버로 전송하는 데이터 프레임에는 M-S간 통신 불량을 넣어 줄 방법이 없음
			// 만약 저장된 데이터를 지우지 않으면 통신 불량 후 정상으로 수신된 데이터와 묶어
			// 전송할 때 통신 불량 직전의 데이터의 검침 시간이 왜곡될 수 있음
			METER_clearStoredData();
		}
	} else {
		Date_t date;
		RTC_read(&date);
		METER_addTempData(&date, &unit);

		if (resultCode == SLAVE_METERING_MT_DOWN) {
			METER_displayTempData();
		} else {
			extern MeterTempData_t TempMeterData;
			TempMeterData.noResponseFromSlave = 1;
		}
	}

	meteringCompleted();
}

#endif

void APP_runMetering(int meteringType)
{
#if defined(AUX_REPEATER)
	if (SLAVE_getAccessState() != SLAVE_ACCESS_IDLE) // 현재 Slave 동작 중일 경우 검침하지 않음.
#else
	if (Metering.inProcess) // 현재 검침중이면 재 검침하지 않음
#endif
	{
		return;
	}

	// INITIAL_METERING 과 PERIODIC_METERING은 무조건 처리하나 즉시 검침은 확인 필요
	if (meteringType == IMMEDIATE_METERING || meteringType == IMMEDIATE_METERING_FOR_REPORT) {
#if defined(AUX_REPEATER)
		if (SLAVE_needToUpdateMeterInfo() ==
		    FALSE) // Meter Info update에 대한 Interval 확인.
#else
		if (METER_needNewMetering(10) ==
		    0) // 즉시 검침의 경우에는 10초 이하이면 재 검침하지 않음.
#endif
		{
			// 즉시 검침의 경우에는 10초 이하이면 재 검침하지 않음
			Metering.meteringType = meteringType;
			meteringCompleted();
			return;
		}
	}

	memset(&Metering, 0, sizeof(Metering));
	Metering.forMeterAdjust = 0;
	Metering.meteringType = meteringType;
	Metering.inProcess = 1;
	Metering.meterType = conf.meterType;
	Metering.success = 0;

	Metering.nRetry = 3;
	if (meteringType == IMMEDIATE_METERING || meteringType == IMMEDIATE_METERING_FOR_REPORT) {
		// 자석을 댄 경우 마지막에 미터 타입을 바꾸어 한 번 더 시도해 보기 위해 4회로 늘림.
		Metering.nRetry = 4;
	}

#if defined(AUX_REPEATER)
	Metering.slaveMeteringError = ERROR_NONE;
	SLAVE_metering(Metering.meteringType);
#else
	METER_sendRequest(Metering.meterType);
	OSAL_startEventTimer(AppTaskId, APP_EVENT_METER_TIMEOUT, (uint32)1500);
#endif
}

// NFC tag 설정 확인 및 복구 code.
void APP_runPeriodicCheckNFC()
{
	NFC_tagDisable();
	NFC_fdDisable();

	PORT1_DIR |= BM(PORT_NFC_TAG);
	PORT1_OUT |= BM(PORT_NFC_TAG);

	if (NFC_checkTagSetting()) {
		printf("NFC TAG setting check - OK\n");
	} else {
		printf("NFC TAG setting check - FAIL\n");
		if (NFC_factoryResetTag()) {
			printf("TAG RESET - OK\n");
		} else {
			printf("TAG RESET - FAIL\n");
		}
	}

	NFC_init();
}

void APP_runPeriodicReport()
{
	if (MODEM_getAccessState() == ACCESS_STATE_IDLE) {
		MODEM_open(APP_PERIODIC_REPORT);
	}
}

void meteringUpdateData(BOOL isSuccess, MeterUnitData_t *unit)
{
	Metering.inProcess = 0;
	Metering.success = isSuccess;

	Date_t date;
	RTC_read(&date);
	switch (Metering.meteringType) {
	case PERIODIC_METERING:
		unit->sec = 0; // 주기검침의 경우 초단위는 0으로 설정한다.
		METER_addStoredData(&date, unit);
		METER_displayStoredData();
		break;
	case PERIODIC_METERING_FOR_SAVE:
		if (conf.periodMode) {
			METER_saveInFlash(&date, unit);
		}
		break;
	default:
		METER_addTempData(&date, unit);
		METER_displayTempData();
		break;
	}

	if (isSuccess) {
		LCD_updateMeterValue((uint8 *)unit);

		if (Metering.meteringType == IMMEDIATE_METERING ||
		    Metering.meteringType == IMMEDIATE_METERING_FOR_REPORT) {
			if (Metering.meterType != conf.meterType) {
				printf("meter type changed (%02x --> %02X)\n", conf.meterType,
				       Metering.meterType);

				conf.meterType = Metering.meterType;
				FLASH_saveConfigInfo(&conf);
				FLASH_readConfigInfo(&conf);
			}
		}
	}
}

void meteringRetry()
{
	LCD_redraw(); // LCD의 프로펠러 회전을 중지하기 위함.

	if (--Metering.nRetry > 0) {
		MISC_delayMs(100); // 약간의 시간 gap을 준 뒤 재검침 시도

		if (Metering.nRetry == 1 &&
		    (Metering.meteringType == IMMEDIATE_METERING ||
		     Metering.meteringType == IMMEDIATE_METERING_FOR_REPORT)) {
			// 마지막 검침에서 미터 타입을 바꾸어 한 번 더 시도함.
			if (Metering.meterType == W_STANDARD_D) {
				Metering.meterType = W_SHINHAN_D;
			} else if (Metering.meterType == W_SHINHAN_D ||
				   Metering.meterType == W_SHINHAN_D_BIG) {
				Metering.meterType = W_STANDARD_D;
			}
		}
		METER_sendRequest(Metering.meterType);
		OSAL_startEventTimer(AppTaskId, APP_EVENT_METER_TIMEOUT, (uint32)1500);
	} else {
		if (Metering.forMeterAdjust) {
			return;
		}
		MeterUnitData_t unit;
		memset(&unit, 0, sizeof(unit));
		memset(unit.meterData, 0xff, 4);
		unit.meterStatus = 0xff;

		meteringUpdateData(FALSE, &unit);
		meteringCompleted();
	}
}

static void meteringRecvResp()
{
	OSAL_stopEventTimer(AppTaskId, APP_EVENT_METER_TIMEOUT);

	if (Metering.forMeterAdjust) {
		Metering.forMeterAdjust = 0;
		METER_bypassResp();
		return;
	}

	MeterUnitData_t unit;
	BOOL result = METER_recvResponse(Metering.meterType, &unit);
	if (result == TRUE) {
		meteringUpdateData(TRUE, &unit);
		meteringCompleted();
#if !defined(AUX_REPEATER)
		if (RTC_isTimeSync()) {
			dataFlash_save((uint8 *)&unit, METER_getMeterCaliber_dp());
		}
#endif
	} else {
		meteringRetry();
	}
}

static void meteringTimeout()
{
	if (conf.sleepMode) {
		APP_prepareToSleep();
		return;
	}

	meteringRetry();
}

event32_t APP_tasks(uint8 taskId, event32_t events)
{
	if (events & APP_EVENT_IMMEDIATE_REPORT) {
		if (!conf.sleepMode) {
			int accessState = MODEM_getAccessState();
			if (accessState == ACCESS_STATE_IDLE) {
				APP_runMetering(IMMEDIATE_METERING_FOR_REPORT);
			} else if (accessState == ACCESS_STATE_WAIT_FEW_SEC) {
				LCD_clear();
				MISC_delayMs(200);
				LCD_redraw();
			}
		}
		return (events ^ APP_EVENT_IMMEDIATE_REPORT);
	}

	if (events & APP_EVENT_RTC_ALARM) {
		RTC_runAlarm();
		return (events ^ APP_EVENT_RTC_ALARM);
	}

	if (events & APP_EVENT_INITIAL) {
		APP_runMetering(INITIAL_METERING);
		return (events ^ APP_EVENT_INITIAL);
	}

	if (events & APP_EVENT_MODEM_RX) {
		MODEM_read();
		return (events ^ APP_EVENT_MODEM_RX);
	}

	if (events & APP_EVENT_MODEM_PROCESS) {
		MODEM_handler();
		return (events ^ APP_EVENT_MODEM_PROCESS);
	}

	if (events & APP_EVENT_MODEM_TIMEOUT) {
		MODEM_timeout();
		return (events ^ APP_EVENT_MODEM_TIMEOUT);
	}

	if (events & APP_EVENT_MODEM_POWER_OFF) {
#if defined(AUX_REPEATER)
		if (SLAVE_isTimeSync() == FALSE && RTC_isTimeSync()) {
			MODEM_close();
			SLAVE_updateTimeSync();
		} else {
			APP_prepareToSleep();
		}
#else
		ctrlReportInterval();
		APP_prepareToSleep();
#endif
		return (events ^ APP_EVENT_MODEM_POWER_OFF);
	}

	if (events & APP_EVENT_METER_RX) {
		METER_disable();
		meteringRecvResp();
		return (events ^ APP_EVENT_METER_RX);
	}

	if (events & APP_EVENT_METER_TIMEOUT) {
		METER_disable();
		meteringTimeout();
		return (events ^ APP_EVENT_METER_TIMEOUT);
	}

	if (events & APP_EVENT_TEST) {
		return (events ^ APP_EVENT_TEST);
	}

	if (events & APP_EVENT_METER_ADJUST) {
		NFCAPP_meterAdjustReq();
		return (events ^ APP_EVENT_METER_ADJUST);
	}

	if (events & APP_EVENT_IMMEDIATE_METERING) {
		APP_runMetering(IMMEDIATE_METERING);
		return (events ^ APP_EVENT_IMMEDIATE_METERING);
	}

	if (events & APP_EVENT_SENSOR_REED) {
		APP_runMetering(IMMEDIATE_METERING);
		return (events ^ APP_EVENT_SENSOR_REED);
	}
	if (events & APP_EVENT_FW_UPDATE) {
		jumpToBSL();
		return (events ^ APP_EVENT_FW_UPDATE);
	}

	if (events & APP_EVENT_NFC_WAIT) {
		NFC_init();
		return (events ^ APP_EVENT_NFC_WAIT);
	}

	if (events & APP_EVENT_REBOOT) {
		REBOOT_SYSTEM();
		return (events ^ APP_EVENT_REBOOT);
	}

	if (events & APP_EVENT_CHANGE_CONFIG) {
		if (SMART_WATER_METER(conf.termModel)) {
			// SWM인 경우 sleep 모드에 따라 H마크 on/off
			METER_std_lcdMarkReq(conf.sleepMode);
			MISC_delayMs(100); // UART 전송완료 대기.
		}
		FLASH_saveConfigInfo(&conf);
		REBOOT_SYSTEM();
		return (events ^ APP_EVENT_CHANGE_CONFIG);
	}

	return (0);
}

#endif

// TDD_TEST
int APP_checkTimeInterval(int h, int bt, int interval)
{
	int result = 0;

	int hour = h + 24 - bt;
	if ((hour % interval) == 0) {
		result = 1; // report required.
	}

	return result;
}

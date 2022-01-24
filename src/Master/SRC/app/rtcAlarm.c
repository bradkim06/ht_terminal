#include <msp430.h>
#include <time.h>

#include "RTC.h"
#include "common_header.h"
#include "MSP430FlashUtil.h"

#include "port_desc.h"
#include "check_meter_misc.h"
#include "uart.h"
#include "lcdDriver.h"
#include "battery.h"

#include "osal_Timer.h"
#include "Task_Mgr.h"

#include "app.h"
#include "flashDriver.h"
#include "meter.h"
#include "rtcAlarm.h"
#include "modem.h"

// 정의된 RTC 타이머 이벤트 개수.
#if defined(AUX_REPEATER)
#define RTC_ALARM_COUNT 1
#else
#define RTC_ALARM_COUNT 3
#endif

#define YEAR_MIN 2017
#define YEAR_MAX 2100

#define ALARM_TYPE_STR(x)                                                                          \
	(((x) == RTC_ALARM_METERING) ? "metering" :                                                \
				       ((x) == RTC_ALARM_REPORT) ?                                 \
				       "report" :                                                  \
				       ((x) == RTC_ALARM_METER_SAVE) ? "meter save" : "unknown")

static int TimeSyncFlag = 0;
static Date_t TimeSyncDate;

static RtcAlarm_t AlarmList[RTC_ALARM_COUNT];
static RtcAlarm_t *NextAlarm = NULL;

int RTC_isValidDate(Date_t *date)
{
	int valid = 0;

	do {
		if (date->year < YEAR_MIN || date->year > YEAR_MAX) {
			break;
		}

		if (date->mon < 1 || date->mon > 12) {
			break;
		}

		if (date->day < 1 || date->day > 31) {
			break;
		}

		if (date->hour > 23 || date->min > 59 || date->sec > 59) {
			break;
		}

		valid = 1;

	} while (0);

	return valid;
}

static void setDefaultRTC(Date_t *date)
{
	date->year = YEAR_MIN;
	date->mon = 1;
	date->day = 1;
	date->hour = 0;
	date->min = 0;
	date->sec = 0;
}

void RTC_read(Date_t *date)
{
	date->year = GetRTCYEAR();
	date->mon = GetRTCMON();
	date->day = GetRTCDAY();
	date->hour = GetRTCHOUR();
	date->min = GetRTCMIN();
	date->sec = GetRTCSEC();
}

void RTC_set(Date_t date)
{
	if (RTC_isValidDate(&date)) {
		RTCCTL01 |= RTCMODE;
		RTCCTL01 &= ~(RTCHOLD);

		SetRTCYEAR(date.year);
		SetRTCMON(date.mon);
		SetRTCDAY(date.day);
		SetRTCHOUR(date.hour);
		SetRTCMIN(date.min);
		SetRTCSEC(date.sec);
	}
}

void RTC_init()
{
	Date_t date;
	RTC_read(&date);

	do {
		if (RTC_isValidDate(&date)) { // CPU 내의 RTC register가 유효한 값.
			break;
		}

		setDefaultRTC(&date); // default 시간 설정
	} while (0);

	RTC_set(date);

	// 보조중계기 및 분단위 검침 시 alaram은 1개만 metering type 및 report min으로 등록.
	// report min에 맞춰 검침 및 보고를 수행. (분단위 동작은 RTC_Alarm_Scheduler_short() 참조)
	if (conf.isShortInterval) {
		AlarmList[0].type = RTC_ALARM_METERING;
		AlarmList[0].minute = conf.reportMin;
#if !defined(AUX_REPEATER)
		AlarmList[1].type = RTC_ALARM_UNKNOWN;
		AlarmList[1].minute = 0;
		AlarmList[2].type = RTC_ALARM_UNKNOWN;
		AlarmList[2].minute = 0;
#endif
	} else {
#if defined(AUX_REPEATER)
		AlarmList[0].type = RTC_ALARM_METERING;
		AlarmList[0].minute = conf.reportMin;
#else
		AlarmList[0].type = RTC_ALARM_METERING;
		AlarmList[0].minute = 0;
		if (conf.reportMin > 30) {
			AlarmList[1].type = RTC_ALARM_METER_SAVE;
			AlarmList[1].minute = 15;
			AlarmList[2].type = RTC_ALARM_REPORT;
			AlarmList[2].minute = conf.reportMin;
		} else {
			AlarmList[1].type = RTC_ALARM_REPORT;
			AlarmList[1].minute = conf.reportMin;
			AlarmList[2].type = RTC_ALARM_METER_SAVE;
			AlarmList[2].minute = 45;
		}
#endif
	}

	TimeSyncFlag = 0;
	memset(&TimeSyncDate, 0, sizeof(Date_t));
}

//알람시간 설정
// 사용하지 않는 파라메타는 0xFF로 설정
// ex: 매일 12:00에 알람을 설정
//      day = 0xFF, hour=12, min=0xFF

// ex: 매월 1일 15:30에 알람을 설정
//      day = 1, hour=15, min=30

static void rtcAlarmSet(uint8 day, uint8 hour, uint8 min)
{
	uint16 alarm_minhr = 0x0000;
	uint16 alarm_dowday = 0x0000;

	//clearing and disable alarm
	RTCCTL01 &= ~(RTCAIE + RTCAIFG);
	RTCAMINHR = 0x0000;
	RTCADOWDAY = 0x0000;

	if (hour != 0xFF) {
		alarm_minhr |= (0x8000 | hour << 8); //alarm enable bit ON
	}

	if (min != 0xFF) {
		alarm_minhr |= (0x0080 | min);
	}

	if (day != 0xFF) {
		alarm_dowday |= (0x8000 | day << 8);
	}

	RTCCTL01 |= RTCAIE;
	RTCAMINHR = alarm_minhr;
	RTCADOWDAY = alarm_dowday;
}

const uint8 MaxDays[12] =
	//  1   2   3   4   5   6   7   8   9  10  11  12
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

void RTC_decDateHour(Date_t *p, uint8 hour)
{
	if (p->hour >= hour) {
		p->hour -= hour;
		return;
	}

	p->hour += (24 - hour);

	if (--p->day > 0) {
		return;
	}

	if (--p->mon > 0) {
		p->day = MaxDays[p->mon - 1];
		if (p->mon == 2 && (p->year % 4) == 0) {
			p->day = 29; // leap year
		}
	} else {
		p->day = 31;
		p->mon = 12;
		p->year--;
	}
}

void RTC_incDateHour(Date_t *p, uint8 hour)
{
	p->hour += hour;
	if (p->hour < 24) {
		return;
	}
	p->hour -= 24;

	uint8 maxDay = MaxDays[p->mon - 1];
	if (p->mon == 2 && (p->year % 4) == 0) {
		maxDay++;
	}

	p->day++;
	if (p->day <= maxDay) {
		return;
	}
	p->day = 1;

	p->mon++;
	if (p->mon <= 12) {
		return;
	}
	p->mon = 1;

	p->year++;
}

void RTC_incDateTime(Date_t *p, uint16 secOffset)
{
	if (p->year < YEAR_MIN || p->year > YEAR_MAX) {
		return;
	}

	if (secOffset == 0 || (secOffset > ((uint16)3600 * (uint16)18))) {
		return;
	}

	uint8 inc_hour = secOffset / 3600;
	secOffset %= 3600;

	uint8 inc_min = secOffset / 60;
	uint8 inc_sec = secOffset % 60;

	p->sec += inc_sec;
	if (p->sec >= 60) {
		p->sec -= 60;
		p->min++;
	}

	p->min += inc_min;
	if (p->min >= 60) {
		p->min -= 60;
		p->hour++;
	}

	p->hour += inc_hour;
	if (p->hour < 24) {
		return;
	}
	p->hour -= 24;

	uint8 maxDay = MaxDays[p->mon - 1];
	if (p->mon == 2 && (p->year % 4) == 0) {
		maxDay++;
	}

	p->day++;
	if (p->day <= maxDay) {
		return;
	}
	p->day = 1;

	p->mon++;
	if (p->mon <= 12) {
		return;
	}
	p->mon = 1;

	p->year++;
}

long RTC_calcSecDiff(Date_t *prev, Date_t *next)
{
	if (RTC_isValidDate(prev) == 0 || RTC_isValidDate(next) == 0) {
		return 0x0FFFFFFF;
	}

	struct tm time;

	time.tm_year = prev->year - 1900;
	time.tm_mon = prev->mon - 1;
	time.tm_mday = prev->day;
	time.tm_hour = prev->hour;
	time.tm_min = prev->min;
	time.tm_sec = prev->sec;

	long prevTime = (uint32)(mktime(&time));

	time.tm_year = next->year - 1900;
	time.tm_mon = next->mon - 1;
	time.tm_mday = next->day;
	time.tm_hour = next->hour;
	time.tm_min = next->min;
	time.tm_sec = next->sec;

	long nextTime = (uint32)(mktime(&time));

	return nextTime - prevTime;
}

int RTC_calcMinDiff(Date_t *prev, Date_t *next)
{
	long secDiff = RTC_calcSecDiff(prev, next);
	return (int)(secDiff / 60);
}

int RTC_calcHourDiff(Date_t *prev, Date_t *next)
{
	uint8 prevMin = prev->min;
	uint8 nextMin = next->min;
	uint8 prevSec = prev->sec;
	uint8 nextSec = next->sec;

	prev->min = next->min = 0;
	prev->sec = next->sec = 0;
	int hourDiff = RTC_calcMinDiff(prev, next) / 60;

	prev->min = prevMin;
	next->min = nextMin;
	prev->sec = prevSec;
	next->sec = nextSec;

	return hourDiff;
}

int RTC_isTimeSync()
{
	return TimeSyncFlag;
}

BOOL RTC_needTimeSync()
{
	// LORA에서는 날자가 바뀌면 새로운 time sync를 실행함.

	Date_t date;
	RTC_read(&date);

	if (TimeSyncFlag && (TimeSyncDate.day == date.day)) {
		return FALSE;
	}

	return TRUE;
}

int RTC_writeTime(Date_t date)
{
	if (RTC_isValidDate(&date)) {
		TimeSyncFlag = 1;
		RTC_set(date);
		memcpy(&TimeSyncDate, &date, sizeof(Date_t));
		return 1;
	}
	return 0;
}

void RTC_Alarm_Scheduler_short()
{
	Date_t date;
	RTC_read(&date);

	// 현재시간이 55초를 초과할 경우 분으로 올림하여 계산.
	if (date.sec > 55) {
		date.min++;
	}
	// 분단위 검침 시 Alarm list[0]만 사용하며, minute을 기반으로 고정된 시간에 수행하도록 schduling한다.
	// ex) AlarmList[0].minute = 8, interval = 3 >> alarm set = 8, 11, 14, 17, ..., 59
	// int ri   = conf.reportInterval;
	int ri = conf.riCtrlValue;
	int hour = date.hour;
	int min = date.min + ri - ((60 + date.min - AlarmList[0].minute) % ri);
	if (min >= 60) {
		min -= 60;
		if (++hour >= 24) {
			hour = 0;
		}
	}

	rtcAlarmSet(0xFF, hour, min);
	NextAlarm = &AlarmList[0];

	printf_ts("set next alarm [%02d:%02d:00] - %s\n\n", hour, min,
		  ALARM_TYPE_STR(NextAlarm->type));
}

void RTC_Alarm_Scheduler_normal()
{
	Date_t date;
	RTC_read(&date);

	// 시간 동기화에 의한 알람 중복 등록을 피하기 위해 이전 알람 이벤트에 의한 순차 등록으로 수정.
	// 단, 시간 동기화 후 발생 오차에 의한 문제를 방지하기 위해 아래와 같은 기준으로 알람을 설정.
	// 1. 알람 발생 후 완료까지 시간 5분.
	// 2. 알람 동작 완료 후 Idle 시간 5분.
	// 3. 시간 오차를 고려하여 실제 알람 시간의 +-1분 오차를 고려.

	int diff, lastDiff = 60;
	RtcAlarm_t *pNext = NULL;
	for (int idx = 0; idx < RTC_ALARM_COUNT; idx++) {
		diff = AlarmList[idx].minute - date.min;
		if (diff <= 0) {
			// diff 값이 음수이거나 차이가 없는 경우 다음 시간에 event를 발생시키기 위해 1시간을 더한다.
			diff += 60;
		}

		printf_ts("set next alarm [%10s:%02dm diff=%02dm] - ",
			  ALARM_TYPE_STR(AlarmList[idx].type), AlarmList[idx].minute, diff);

		// diff 값이 0이거나 1인 알람을 설정할 경우 알람이 발생하지 않을 수 있으므로 제외한다.
		if (diff > 1) {
			if (diff <= lastDiff) {
				pNext = &AlarmList[idx];
				lastDiff = diff;

				printf("change\n");
			} else {
				printf("skip\n");
			}
		} else {
			printf("diff is too small \n");
		}
	}

	// 검색된 next alarm이 없는 경우 첫번째 알람을 설정한다.
	if (pNext) {
		NextAlarm = pNext;
	} else {
		printf_ts("set next alarm - set fail, set first alarm \n");
		NextAlarm = &AlarmList[0];
	}

	int hour = NextAlarm->minute <= date.min ? ((date.hour + 1) % 24) : date.hour;
	int min = NextAlarm->minute;
	rtcAlarmSet(0xFF, hour, min);

	printf_ts("set next alarm [%02d:%02d:00] - %s\n", hour, min,
		  ALARM_TYPE_STR(NextAlarm->type));
}

void RTC_alarmSchedule()
{
	if (conf.isShortInterval) {
		RTC_Alarm_Scheduler_short();
	} else {
		RTC_Alarm_Scheduler_normal();
	}
}

void RTC_runAlarm()
{
	if (NextAlarm == NULL) {
		APP_prepareToSleep();
		return;
	}

	printf_ts("alarm occurred - '%s'%s\n", ALARM_TYPE_STR(NextAlarm->type),
		  (conf.sleepMode) ? "(sleep)" : "(active)");
	if (conf.sleepMode) {
		MISC_delayMs(1500); // sleep mode 인 경우 1.5sec 대기 후 다시 LPM3준비
		APP_prepareToSleep();
		return;
	}

	if (conf.isShortInterval) {
		APP_runMetering(PERIODIC_METERING);
		return;
	}

	Date_t date;
	RTC_read(&date);

	RtcAlarmType_t alarmType = NextAlarm->type;
#if defined(AUX_REPEATER)
	if (alarmType == RTC_ALARM_METERING) {
		if (APP_checkTimeInterval(date.hour, conf.intervalBaseTime, conf.riCtrlValue)) {
			APP_runMetering(PERIODIC_METERING);
		} else {
			APP_prepareToSleep();
		}
	}
#else
	if (alarmType == RTC_ALARM_REPORT) {
		if (APP_checkTimeInterval(date.hour, conf.intervalBaseTime, conf.riCtrlValue)) {
			APP_runPeriodicReport();
		} else {
			APP_prepareToSleep();
		}
	} else if (alarmType == RTC_ALARM_METERING) {
		if (APP_checkTimeInterval(date.hour, conf.intervalBaseTime, conf.meterInterval)) {
			APP_runMetering(PERIODIC_METERING);
		} else {
			APP_prepareToSleep();
		}
	} else if (alarmType == RTC_ALARM_METER_SAVE) {
		if (conf.periodMode && RTC_isTimeSync()) {
			// 기간검침 데이터 저장 Base Time을 "통신 Base Time + 1시간" 으로 설정.
			// 해당 이유는 LoRa 통신 문제 발생 시 Join시도를 1시간동안 하므로
			// 경우에 따라 0시에 기간검침 데이터를 저장하지 않는 문제가 발생하므로 수정.
			if ((date.hour % METER_DATA_SAVE_INTERVAL) == (conf.intervalBaseTime + 1)) {
				APP_runMetering(PERIODIC_METERING_FOR_SAVE);
			} else {
				APP_prepareToSleep();
			}
		} else {
			APP_prepareToSleep();
		}
	}

#endif
	else {
		printf_ts("unknow alarm type(%d)\n", alarmType);
		APP_prepareToSleep();
	}
}

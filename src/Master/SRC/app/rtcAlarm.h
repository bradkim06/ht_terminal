#ifndef __RTC_ALARM_H__
#define __RTC_ALARM_H__

#include "common_header.h"
#include "meter.h"

#define YEAR_MIN 2017
#define YEAR_MAX 2100

// RTC 타이머 이벤트(= Alarm)
typedef enum {
	RTC_ALARM_METERING, // 매 시간 0분에 발생하는 검침 이벤트
	RTC_ALARM_REPORT, // 매 시간 정해진 시간(serial number에 따라 변경)에 발생하는 보고 이벤트
	RTC_ALARM_METER_SAVE, // 매 시간 15분 or 45분에 발생하는 검침 데이터 저장 이벤트 (REPORT 시간에따라 결정됨.)
	RTC_ALARM_UNKNOWN, // 미사용 알람
} RtcAlarmType_t;

typedef struct {
	uint8 minute;
	RtcAlarmType_t type;
} RtcAlarm_t;

void RTC_set(Date_t date);
void RTC_read(Date_t *date);
void RTC_init();
void RTC_alarmSchedule(void);
void RTC_incDateHour(Date_t *p, uint8 hour);
void RTC_decDateHour(Date_t *p, uint8 hour);
void RTC_incDateTime(Date_t *p, uint16 secOffset);
void RTC_runAlarm();
int RTC_calcMinDiff(Date_t *prev, Date_t *next);
int RTC_calcHourDiff(Date_t *prev, Date_t *next);
long RTC_calcSecDiff(Date_t *prev, Date_t *next);
int RTC_isValidDate(Date_t *date);
int RTC_writeTime(Date_t date);
int RTC_isTimeSync();
BOOL RTC_needTimeSync();

#endif // __RTC_ALARM_H__

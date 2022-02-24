#ifndef RTC_H_
#define RTC_H_

struct Time {
	int Year, Month, DayOfWeek, Day, Hour, Minute, Second;
};

int TestRTCYear(struct Time TaD);
int TestRTCMonth(struct Time TaD);
int TestRTCDow(struct Time TaD);
int TestRTCDay(struct Time TaD);
int TestRTCHour(struct Time TaD);
int TestRTCMinute(struct Time TaD);
int TestRTCSecond(struct Time TaD);

#endif /*RTC_H_*/

///////////////////////////////////////////////////////////////////////////////
//
//    Battery Charge Control
//
///////////////////////////////////////////////////////////////////////////////

#include "common_header.h"
#include "RTC.h"
#include "port_desc.h"
#include "check_meter_misc.h"
#include "battery.h"
#include "app.h"

//배터리 전압
#define BATTERY_AVR_NUM 6
#define BATTERY_MAX_ADC 4096
#define BATTERY_MIN_ADC 0

static uint16 BattAdcBuffer[BATTERY_AVR_NUM];
static uint8 LastBattVolt = 0;

static uint16 readBatteryADC(void)
{
	uint16 value;
	volatile int i;
	const int ADC_LOOP_CNT = 6;

	uint16 sum = 0;
	uint16 max_adc = BATTERY_MIN_ADC,
	       min_adc = BATTERY_MAX_ADC; //최대, 최소값 제거를 위한 초기화

	PORT_ADC_DIR &= ~BM(BATTERY_ADC_IN);
	PORT_ADC_REN &= ~BM(BATTERY_ADC_IN);

	MISC_delayMs(40);

	for (int loop = 0; loop < ADC_LOOP_CNT; loop++) {
		for (i = 0; i < 100; i++)
			;

		//ADC12CTL0 = ADC12ON+ADC12SHT0_1+ADC12REFON+ADC12REF2_5V;         // Turn on and set up ADC12
		ADC12CTL0 = ADC12ON + ADC12SHT0_1 + ADC12REFON; // Turn on and set up ADC12
		REFCTL0 = REFON; //Ref volt = 1.5V

		ADC12CTL1 = ADC12SHP; // Use sampling timer

		ADC12MCTL0 = ADC12SREF_1 + ADC12INCH_0; // Input A0 port,범위 1.5V - 0V

		for (i = 0; i < 100; i++)
			; // Delay to allow Ref to settle

		ADC12CTL0 |= (ADC12ENC | ADC12SC); // Start conversion

		for (i = 0; i < 100; i++) {
			if (ADC12IFG & BIT0) {
				break;
			} else {
				MISC_delayMs(1);
			}
		}

		value = ADC12MEM0;
		sum += value;

		if (max_adc < value)
			max_adc = value;

		if (min_adc > value)
			min_adc = value;

		ADC12CTL0 = 0x0000;
		ADC12CTL0 &= ~(ADC12ON | ADC12REFON | ADC12SHT0_1);
		ADC12CTL1 = 0x0000;
		ADC12MCTL0 = 0x00;
	}

	sum = sum - (max_adc + min_adc);

	//value = sum / (ADC_LOOP_CNT - 2);
	value = sum >> 2; // 4로 나누기

	//4096 = 1.5V

	PORT_ADC_DIR |= BM(BATTERY_ADC_IN);
	PORT_ADC_REN |= BM(BATTERY_ADC_IN);

	return (value);
}

void addBatteryADC(void)
{
	int i;
	uint16 batt = 0;
	uint16 sum = 0;

	for (i = 0; i < 2; i++) {
		sum += readBatteryADC();
	}

	batt = sum >> 1; // 2로 나누기

	if (batt > 0) {
		for (i = 1; i < BATTERY_AVR_NUM; i++) {
			BattAdcBuffer[i - 1] = BattAdcBuffer[i];
		}

		BattAdcBuffer[BATTERY_AVR_NUM - 1] = batt;
	}
}

void BATT_init()
{
	int i;

	for (i = 0; i < BATTERY_AVR_NUM; i++) {
		BattAdcBuffer[i] = readBatteryADC();
	}

	BATT_loadLastVoltage();
}

//yikim 시간 문제로 미리 수행
uint8 BATT_getLastVoltage(void)
{
	return LastBattVolt;
}

void BATT_loadLastVoltage(void)
{
	LastBattVolt = BATT_getVoltage();
}

uint8 BATT_getVoltage(void)
{
	int i;
	uint16 max_adc = BATTERY_MIN_ADC,
	       min_adc = BATTERY_MAX_ADC; //최대, 최소값 제거를 위한 초기화
	uint16 total_adc = 0, avr_adc = 0;

	addBatteryADC();

	for (i = 0; i < BATTERY_AVR_NUM; i++) {
		total_adc += BattAdcBuffer[i];

		if (max_adc < BattAdcBuffer[i])
			max_adc = BattAdcBuffer[i];

		if (min_adc > BattAdcBuffer[i])
			min_adc = BattAdcBuffer[i];
	}

	avr_adc = total_adc - (max_adc + min_adc);
	avr_adc = avr_adc >> 2; // 4로 나누기
	avr_adc /= 99;

	return (uint8)avr_adc;
}

int BATT_getLevel()
{
	int level = 0;

	int battery = BATT_getVoltage();
	if (battery > 30) {
		level = 3;
	} else if (battery > 28) {
		level = 2;
	} else if (battery > 26) {
		level = 1;
	} else {
		level = 0;
	}

	return level;
}

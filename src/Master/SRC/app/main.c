#include <msp430.h>
#include "common_header.h"
#include "osal_Timer.h"
#include "Task_Mgr.h"
#include "port_desc.h"
#include "uart.h"
#include "check_meter_misc.h"
#include "lcdDriver.h"
#include "test.h"
#include "NFC_i2c.h"
#include "battery.h"

static BOOL IsTestModeOn = FALSE;

static BOOL checkTestMode()
{
	// P1.0 --> if 0 with inernal pull-up, test mode
	//          else normal mode

	P1DIR |= 0x01;
	P1REN |= 0x01;
	P1OUT |= 0x01;

	P1DIR &= ~0x01;

	MISC_delayUs(1); // specific case need this delay

	BOOL IsTestMode = (P1IN & 0x01) ? FALSE : TRUE;

	P1DIR |= 0x01;
	P1OUT &= ~0x01;

	return IsTestMode;
}

void initClock(void)
{
	UCSCTL3 |= SELREF_2; // Set DCO FLL reference = REFO
	P7SEL |= 0x03;

	//UCSCTL6 = 0;
	UCSCTL6 &= ~(XT1OFF); // XT1 On
	UCSCTL6 |= XCAP_3; // Internal load cap

	UCSCTL4 &= 0xF8FF;
	UCSCTL4 |= SELA_0; // Set ACLK = XTCLK

	__bis_SR_register(SCG0); // Disable the FLL control loop
	UCSCTL0 = 0x0000; // Set lowest possible DCOx, MODx
	UCSCTL1 = DCORSEL_6; // Select DCO range 8MHz operation
	UCSCTL2 = FLLD_1 + 249; // Set DCO Multiplier for 8MHz
		// (N + 1) * FLLRef = Fdco
		// (249 + 1) * 32768 = 8MHz
		// Set FLL Div = fDCOCLK/2
	__bic_SR_register(SCG0); // Enable the FLL control loop

	// Worst-case settling time for the DCO when the DCO range bits have been
	// changed is n x 32 x 32 x f_MCLK / f_FLL_reference. See UCS chapter in 5xx
	// UG for optimization.
	// 32 x 32 x 8 MHz / 32,768 Hz = 250000 = MCLK cycles for DCO to settle
	__delay_cycles(250000);

	// Loop until XT1,XT2 & DCO fault flag is cleared

	BOOL isSetInternalClock = TRUE;
	for (int loop = 0; loop < 10000; loop++) {
		__delay_cycles(2500);
		UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + XT1HFOFFG + DCOFFG);
		SFRIFG1 &= ~OFIFG; // Clear fault flags
		if ((SFRIFG1 & OFIFG) == 0) {
			isSetInternalClock = FALSE;
			break;
		}
	}

	// If "ACLK = XTCLK" setting failed, set "ACLK = REFO"
	// XTCLK(= External OSC 32KHz), REFO(=Internal OSC 32KHz)
	if (isSetInternalClock == TRUE) {
		UCSCTL4 |= SELA_2; // Set ACLK = REFO
	} else {
		// OSC fault, Vacant memory access, NMI pin interrupt,
		SFRIE1 |= (OFIE + VMAIE + NMIIE);
	}

	RTCCTL2 |= 0x80;
	RTCCTL01 |= RTCMODE;
}

void initPort()
{
#if LORA_DEVICE
	P1SEL = 0x00;
	P1DIR = 0xFF;
	P1OUT = 0x00;

	// P2.5 (BOOT0)  : Module BOOT0 pin, Input
	// P2.3 (NRESET) : Module nReset pin, output high
	// P2.0 (VCC)    : change power on
	P2SEL = 0x00;
	P2DIR = 0xDF;
	P2OUT = 0x08;
	// P2OUT = 0x01;
#else
	P1SEL = 0x00;
	P1DIR = 0xFF;
	P1OUT = 0x00; // All set output low

	P2SEL = 0x00;
	P2DIR = 0xFF;
#if (DEVICE_REVISION == DEV_REV_PWRCTRL_MOSFET_ONLY)
	// P2.5 (MCU_RESET) : NC �Ǿ����Ƿ� ouput low
	// P2.3 (NRESET)    : NC �Ǿ����Ƿ� ouput low
	// P2.0 (LDO_EN)    : power off Modem (output high)
	P2OUT = 0x01;
#else
	// P2.5 (MCU_RESET) : NC �Ǿ����Ƿ� ouput low
	// P2.3 (NRESET)    : NC �Ǿ����Ƿ� ouput low
	// P2.0 (LDO_EN)    : power off Modem (output low)
	P2OUT = 0x00;
#endif
#endif
	P3SEL = 0x00;
	P3DIR = 0xFF;
	P3OUT = 0x00;

	P4SEL = 0x00;
	P4DIR = 0xFF;
	P4OUT = 0x00;

#if defined(AUX_REPEATER)
	// �ʱ� CC1200 power off
	P4SEL &= ~0x10;
	P4DIR |= 0x10;
	P4OUT |= 0x10;
#endif

	P5SEL = 0x00;
	P5DIR = 0xFF;
	P5OUT = 0x00;

	P6SEL = 0x00;
	P6DIR = 0xFF;
	P6OUT = 0x00;

	P7SEL = 0x03; // Xin, Xout
	P7DIR = 0xFF;
	P7OUT = 0x00;

	P8SEL = 0x00;
	P8DIR = 0xFF;
	P8OUT = 0x00;

	P9SEL = 0x00;
	P9DIR = 0xFF;
	P9OUT = 0x00;

	P10SEL = 0x00;
	P10DIR = 0xFF;
	P10OUT = 0x00;

	P11SEL = 0x00;
	P11DIR = 0xFF;
	P11OUT = 0x00;

	PFSEL = 0x00;
	PFDIR = 0xFF;
	PFOUT = 0x00;

	//    PJSEL = 0x00;
	PJDIR = 0xFF;
	PJOUT = 0x00;

	//Interrut disable
	P1IE = 0x00;
	P2IE = 0x00;

	P1IFG = 0x00;
	P2IFG = 0x00;

	//Reed Sensor
	if (LCD_Exist()) {
		PORT_SENSOR_DIR &= ~BM(PORT_SENSOR_REED); // Set to Input
		PORT_SENSOR_IE |= BM(PORT_SENSOR_REED); // Interrupt enabled
		PORT_SENSOR_IES &= ~BM(PORT_SENSOR_REED); // Low/Hi edge
		PORT_SENSOR_IFG &= ~BM(PORT_SENSOR_REED); // IFG cleared

#if defined(AUX_REPEATER)
		if (MISC_findPushButton()) {
			PORT1_DIR &= ~BM(PORT_LCD_SWITCH); // Set to Input
			PORT1_IE |= BM(PORT_LCD_SWITCH); // Interrupt enabled
			PORT1_IES |= BM(PORT_LCD_SWITCH); // Hi/Low edge
			PORT1_IFG &= ~BM(PORT_LCD_SWITCH); // IFG cleared
		}
#endif
	}

#if (DEVICE_REVISION == DEV_REV_PWRCTRL_MOSFET_LDO)
	if (MISC_getBslType()) {
		P1DIR = 0xF9; // set P1.1 P1.2 as input
		P1OUT &= ~0x06;
		P2OUT |= BM(7);
		P4OUT = 0x40; // P4.6 (MOSFET_EN) : power off Modem
	} else {
		P1OUT = 0x02; // P1.1 (MOSFET_EN) : power off Modem
	}
#endif

	PORT_ADC_DIR |= BM(BATTERY_ADC_IN);
	PORT_ADC_REN |= BM(BATTERY_ADC_IN);
}

void initSystem()
{
	initPort();
	initClock();

	GLOBAL_ENABLE_INT();

	UART_init();
	TIMER_init();
	TIMER_start();

#define TEST_WAIT_DELAY (100)
#define TEST_CHECK_COUNT (500 / TEST_WAIT_DELAY)
	for (int i = 0; i < TEST_CHECK_COUNT; i++) {
		IsTestModeOn = checkTestMode();
		if (IsTestModeOn == FALSE) {
			break;
		}
		MISC_delayMs(TEST_WAIT_DELAY);
	}
}

#pragma CODE_SECTION(main, "MAIN")
int main()
{
	WDTCTL = WDTPW | WDTHOLD;
#define FOTA_RAM_ADDR 0x1C00
#define FOTA_FLASH_ADDR 0x20000
#define FOTA_SIZE 0xC00
	// isr copy ram (size & addr need to check lnk.cmd)
	memcpy((void *)FOTA_RAM_ADDR, (void *)FOTA_FLASH_ADDR, FOTA_SIZE);
	WDTCTL = WDT_VRST_50SEC; // Start watchdog timer(3.2768 sec)

	initSystem();

	// 시스템 초기화 및 테스트모드 여부 확인 후 WDT는 16초로 재설정
	// 1. App level의 코드 구동 시 Console 출력이 포함되어 동작 시간이 유동적으로 변함.
	// 2. 시스템 초기화 이후에는 Clock 설정이 완료된 상태이므로 WDT를 재설정해도 무방.

	WDTCTL = WDT_ARST_16SEC; // Start watchdog timer(16 sec)
	if (IsTestModeOn) {
		TEST_checkTestMode();
		TEST_run();
	} else {
		TASKMGR_init();
		TASKMGR_run();
	}
}

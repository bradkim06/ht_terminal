#ifndef _PORT_DESC_HEADER_
#define _PORT_DESC_HEADER_

#include <msp430.h>

//=======================================================
//  PORT Description
//=======================================================

//============================================
//PORT1 -DIPSW
//============================================

#define PORT_DIPSW_DIR P1DIR
#define PORT_DIPSW_IN P1IN

#define PORT1_DIR P1DIR
#define PORT1_SEL P1SEL
#define PORT1_REN P1REN
#define PORT1_OUT P1OUT
#define PORT1_IN P1IN
#define PORT1_IE P1IE
#define PORT1_IES P1IES
#define PORT1_IFG P1IFG

#define PORT_NFC_TAG MISC_NfcPortSelect() // P1.7
#define PORT_NFC_FD_IN 5 // P1.5

#define PORT4_DIR P4DIR
#define PORT4_SEL P4SEL
#define PORT4_REN P4REN
#define PORT4_OUT P4OUT
#define PORT4_IN P4IN
#define PORT4_IE P4IE
#define PORT4_IES P4IES
#define PORT4_IFG P4IFG

#if LORA_DEVICE
#define PORT_NFC_VCC 0 //  P 4.0  TP117
#else
#define PORT_NFC_VCC 7 //  P 4.7  TP117
#endif

//============================================
// PORT2 SENSOR,PULSE
//============================================
#define PORT_SENSOR_DIR P2DIR
#define PORT_SENSOR_SEL P2SEL
#define PORT_SENSOR_IE P2IE
#define PORT_SENSOR_IES P2IES
#define PORT_SENSOR_IFG P2IFG

#define PORT_MODEM_POWER 0
#define PORT_SENSOR_REED 1
#define PORT_LCD_SWITCH 6
#define PORT_PULSE_DATA_1 3
#define PORT_CC1200_GPIO3 0 // Not use this pin (old version)
#define PORT_CC1200_GPIO0 7
//============================================
//PORT3 - G/W UARTA0
//============================================
#define PORT_GW_DIR P3DIR
#define PORT_GW_SEL P3SEL
#define PORT_GW_TXD 4
#define PORT_GW_RXD 5

#define GW_UART_BUSY_CHECK()                                                                       \
	while (!(UCA0IFG & UCTXIFG))                                                               \
		;
#define GW_UART_WRITE(x) UCA0TXBUF = x;

//============================================
// METER PORT
//    INT UART1   : PORT 4.6
//    INT UART2   : PORT 4.4 (current not use)
//    RF424 POWER : PORT 4.4
//============================================
#define PORT_METER_INT_DIR P4DIR
#define PORT_METER_INT_SEL P4SEL
#define PORT_METER_INT_OUT P4OUT
#define PORT_METER_INT_REN P4REN
#define PORT_UART1_INT_PIN 6
#define PORT_UART2_INT_PIN 4 // Not use this pin (old version)
#define PORT_CC1200_POWER 4

//============================================
// UART PORT_1 RX
//    DATA : PORT10.5 - UCA3
//============================================
#define PORT_UART1_DATA_DIR P10DIR
#define PORT_UART1_DATA_SEL P10SEL
#define PORT_UART1_DATA_REN P10REN
#define PORT_UART1_DATA_OUT P10OUT
#define PORT_UART1_DATA_RXD 5

//============================================
// UART PORT_2 RX
//    DATA : PORT 9.5 - UCA2
//============================================
#define PORT_UART2_DATA_DIR P9DIR
#define PORT_UART2_DATA_SEL P9SEL
#define PORT_UART2_DATA_REN P9REN
#define PORT_UART2_DATA_OUT P9OUT
#define PORT_UART2_DATA_RXD 5

//============================================
// PORT5 - DEBUG UARTA0
//============================================

#define PORT_DBG_DIR P5DIR
#define PORT_DBG_SEL P5SEL
#define PORT_DBG_TXD 6 // P5.6 : output,
#define PORT_DBG_RXD 7 // P5.7 : input, R1OUT(RS232 receiver outp)

//============================================
// PORT6 - ADC
//============================================
#define PORT_ADC_DIR P6DIR
#define PORT_ADC_SEL P6SEL
#define PORT_ADC_REN P6REN
#define PORT_ADC_IN P6IN
#define PORT_ADC_OUT P6OUT

#define BATTERY_ADC_IN 0

//============================================
// PORT8 - RS232 EN
//============================================
#define PORT_METER_RS232_DIR P8DIR
#define PORT_METER_RS232_OUT P8OUT

#define PORT_METER_RS232_TX_EN 1 // P8.1 : output, TE232(RS232 transmitter output enable)
#define PORT_METER_RS232_RX_EN 2 // P8.2 : output, RE232(RS232 receiver enable)

//============================================
// PORT7 - RF
//============================================

// port 3
#define PORT_RF_POWER_DIR P4DIR
#define PORT_RF_POWER_REN P4REN
#define PORT_RF_POWER_SEL P4SEL
#define PORT_RF_POWER_IN P4IN
#define PORT_RF_POWER_OUT P4OUT

#define PORT_RF_POWER_PIN 0 //

//============================================
// PORT10 - NFC
//============================================
#define PORT10_DIR P10DIR
#define PORT10_REN P10REN
#define PORT10_SEL P10SEL
#define PORT10_IN P10IN
#define PORT10_OUT P10OUT

#define PORT_NFC_SDA 1
#define PORT_NFC_SCL 2

////////////////////////////////////////////

#endif

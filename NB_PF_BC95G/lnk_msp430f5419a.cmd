/* ============================================================================ */
/* Copyright (c) 2019, Texas Instruments Incorporated                           */
/*  All rights reserved.                                                        */
/*                                                                              */
/*  Redistribution and use in source and binary forms, with or without          */
/*  modification, are permitted provided that the following conditions          */
/*  are met:                                                                    */
/*                                                                              */
/*  *  Redistributions of source code must retain the above copyright           */
/*     notice, this list of conditions and the following disclaimer.            */
/*                                                                              */
/*  *  Redistributions in binary form must reproduce the above copyright        */
/*     notice, this list of conditions and the following disclaimer in the      */
/*     documentation and/or other materials provided with the distribution.     */
/*                                                                              */
/*  *  Neither the name of Texas Instruments Incorporated nor the names of      */
/*     its contributors may be used to endorse or promote products derived      */
/*     from this software without specific prior written permission.            */
/*                                                                              */
/*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" */
/*  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,       */
/*  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR      */
/*  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR            */
/*  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,       */
/*  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,         */
/*  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; */
/*  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,    */
/*  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR     */
/*  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,              */
/*  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                          */
/* ============================================================================ */

/******************************************************************************/
/* lnk_msp430f5419a.cmd - LINKER COMMAND FILE FOR LINKING MSP430F5419A PROGRAMS     */
/*                                                                            */
/*   Usage:  lnk430 <obj files...>    -o <out file> -m <map file> lnk.cmd     */
/*           cl430  <src files...> -z -o <out file> -m <map file> lnk.cmd     */
/*                                                                            */
/*----------------------------------------------------------------------------*/
/* These linker options are for command line linking only.  For IDE linking,  */
/* you should set your linker options in Project Properties                   */
/* -c                                               LINK USING C CONVENTIONS  */
/* -stack  0x0100                                   SOFTWARE STACK SIZE       */
/* -heap   0x0100                                   HEAP AREA SIZE            */
/*                                                                            */
/*----------------------------------------------------------------------------*/
/* Version: 1.207                                                             */
/*----------------------------------------------------------------------------*/

/****************************************************************************/
/* Specify the system memory map                                            */
/****************************************************************************/
/* FLASHB boundaries have been changed to fix CPU20 (see errata sheet)      */
#define METER_ADDR 0x10200
#define METER_LEN 0x1C00

#define APP_ADDR 0x10200 + METER_LEN
#define APP_LEN 0x1000

#define NFC_PROTOCOL_ADDR APP_ADDR + APP_LEN
#define NFC_PROTOCOL_LEN 0x1200

#define RTC_ALARM_ADDR NFC_PROTOCOL_ADDR + NFC_PROTOCOL_LEN
#define RTC_ALARM_LEN 0xA00

#define MAIN_ADDR RTC_ALARM_ADDR + RTC_ALARM_LEN
#define MAIN_LEN 0x200

#define DATA_FLASH_ADDR MAIN_ADDR + MAIN_LEN
#define DATA_FLASH_LEN 0x1800

#define UART_ADDR DATA_FLASH_ADDR + DATA_FLASH_LEN
#define UART_LEN 0xC00

#define NFC_I2C_ADDR UART_ADDR + UART_LEN
#define NFC_I2C_LEN 0xA00

#define FLASH_DRIVER_ADDR NFC_I2C_ADDR + NFC_I2C_LEN
#define FLASH_DRIVER_LEN 0xA00

#define LCD_DRIVER_ADDR FLASH_DRIVER_ADDR + FLASH_DRIVER_LEN
#define LCD_DRIVER_LEN 0x400

#define FLASH_UTIL_ADDR LCD_DRIVER_ADDR + LCD_DRIVER_LEN
#define FLASH_UTIL_LEN 0x200

#define BATTERY_ADDR FLASH_UTIL_ADDR + FLASH_UTIL_LEN
#define BATTERY_LEN 0x200

#define METER_MISC_ADDR BATTERY_ADDR + BATTERY_LEN
#define METER_MISC_LEN 0x200

#define OSAL_TIMER_ADDR METER_MISC_ADDR + METER_MISC_LEN
#define OSAL_TIMER_LEN 0x400

#define TASK_MGR_ADDR OSAL_TIMER_ADDR + OSAL_TIMER_LEN
#define TASK_MGR_LEN 0x200

#define NB_PROCESS_ADDR TASK_MGR_ADDR + TASK_MGR_LEN
#define NB_PROCESS_LEN 0x1C00

#define NB_RESPONSE_ADDR NB_PROCESS_ADDR + NB_PROCESS_LEN
#define NB_RESPONSE_LEN 0x1400

#define MESSAGE_ADDR NB_RESPONSE_ADDR + NB_RESPONSE_LEN
#define MESSAGE_LEN 0x1200

#define MODEM_ADDR MESSAGE_ADDR + MESSAGE_LEN
#define MODEM_LEN 0x1000

#define ISR_ADDR 0x1C00
#define ISR_LEN 0x600

#define RAM_ADDR ISR_ADDR + ISR_LEN

MEMORY
{
    SFR                     : origin = 0x0000, length = 0x0010
    PERIPHERALS_8BIT        : origin = 0x0010, length = 0x00F0
    PERIPHERALS_16BIT       : origin = 0x0100, length = 0x0100
    ISR                     : origin = ISR_ADDR, length = ISR_LEN
    RAM                     : origin = RAM_ADDR, length = 0x3A00
    INFOA                   : origin = 0x1980, length = 0x0080
    INFOB                   : origin = 0x1900, length = 0x0080
    INFOC                   : origin = 0x1880, length = 0x0080
    INFOD                   : origin = 0x1800, length = 0x0080
    MAIN_CODE	        	: origin = 0x5C00, length = 0x0200
    TEST                    : origin = 0x5E00, length = 0x5C00
    FLASHB                  : origin = 0xBA00, length = 0x45D2
    /* METER                   : origin = METER_ADDR, length = METER_LEN */
    /* APP                     : origin = APP_ADDR, length = APP_LEN */
    /* NFC_PROTOCOL            : origin = NFC_PROTOCOL_ADDR, length = NFC_PROTOCOL_LEN */
    /* RTC_ALARM               : origin = RTC_ALARM_ADDR, length = RTC_ALARM_LEN */
    /* MAIN_TEXT               : origin = MAIN_ADDR, length = MAIN_LEN */
    /* DATA_FLASH              : origin = DATA_FLASH_ADDR, length = DATA_FLASH_LEN */
    /* UART                    : origin = UART_ADDR, length = UART_LEN */
    /* NFC_I2C                 : origin = NFC_I2C_ADDR, length = NFC_I2C_LEN */
    /* FLASH_DRIVER            : origin = FLASH_DRIVER_ADDR, length = FLASH_DRIVER_LEN */
    /* LCD_DRIVER              : origin = LCD_DRIVER_ADDR, length = LCD_DRIVER_LEN */
    /* FLASH_UTIL              : origin = FLASH_UTIL_ADDR, length = FLASH_UTIL_LEN */
    /* BATTERY                 : origin = BATTERY_ADDR, length = BATTERY_LEN */
    /* METER_MISC              : origin = METER_MISC_ADDR, length = METER_MISC_LEN */
    /* OSAL_TIMER              : origin = OSAL_TIMER_ADDR, length = OSAL_TIMER_LEN */
    /* TASK_MGR                : origin = TASK_MGR_ADDR, length = TASK_MGR_LEN */
    /* NB_PROCESS              : origin = NB_PROCESS_ADDR, length = NB_PROCESS_LEN */
    /* NB_RESPONSE             : origin = NB_RESPONSE_ADDR, length = NB_RESPONSE_LEN */
    /* MESSAGE                 : origin = MESSAGE_ADDR, length = MESSAGE_LEN */
    /* MODEM                   : origin = MODEM_ADDR, length = MODEM_LEN */
    FLASHC                  : origin = 0x10200,length = 0xFE00
    FLASHD                  : origin = 0x20000,length = 0x3300
    /* INT00                   : origin = 0xFF80, length = 0x0002 */
    /* INT01                   : origin = 0xFF82, length = 0x0002 */
    /* INT02                   : origin = 0xFF84, length = 0x0002 */
    /* INT03                   : origin = 0xFF86, length = 0x0002 */
    /* INT04                   : origin = 0xFF88, length = 0x0002 */
    /* INT05                   : origin = 0xFF8A, length = 0x0002 */
    /* INT06                   : origin = 0xFF8C, length = 0x0002 */
    /* INT07                   : origin = 0xFF8E, length = 0x0002 */
    /* INT08                   : origin = 0xFF90, length = 0x0002 */
    /* INT09                   : origin = 0xFF92, length = 0x0002 */
    /* INT10                   : origin = 0xFF94, length = 0x0002 */
    /* INT11                   : origin = 0xFF96, length = 0x0002 */
    /* INT12                   : origin = 0xFF98, length = 0x0002 */
    /* INT13                   : origin = 0xFF9A, length = 0x0002 */
    /* INT14                   : origin = 0xFF9C, length = 0x0002 */
    /* INT15                   : origin = 0xFF9E, length = 0x0002 */
    /* INT16                   : origin = 0xFFA0, length = 0x0002 */
    /* INT17                   : origin = 0xFFA2, length = 0x0002 */
    /* INT18                   : origin = 0xFFA4, length = 0x0002 */
    /* INT19                   : origin = 0xFFA6, length = 0x0002 */
    /* INT20                   : origin = 0xFFA8, length = 0x0002 */
    /* INT21                   : origin = 0xFFAA, length = 0x0002 */
    /* INT22                   : origin = 0xFFAC, length = 0x0002 */
    /* INT23                   : origin = 0xFFAE, length = 0x0002 */
    /* INT24                   : origin = 0xFFB0, length = 0x0002 */
    /* INT25                   : origin = 0xFFB2, length = 0x0002 */
    /* INT26                   : origin = 0xFFB4, length = 0x0002 */
    /* INT27                   : origin = 0xFFB6, length = 0x0002 */
    /* INT28                   : origin = 0xFFB8, length = 0x0002 */
    /* INT29                   : origin = 0xFFBA, length = 0x0002 */
    /* INT30                   : origin = 0xFFBC, length = 0x0002 */
    /* INT31                   : origin = 0xFFBE, length = 0x0002 */
    /* INT32                   : origin = 0xFFC0, length = 0x0002 */
    /* INT33                   : origin = 0xFFC2, length = 0x0002 */
    /* INT34                   : origin = 0xFFC4, length = 0x0002 */
    /* INT35                   : origin = 0xFFC6, length = 0x0002 */
    /* INT36                   : origin = 0xFFC8, length = 0x0002 */
    /* INT37                   : origin = 0xFFCA, length = 0x0002 */
    /* INT38                   : origin = 0xFFCC, length = 0x0002 */
    /* INT39                   : origin = 0xFFCE, length = 0x0002 */
    /* INT40                   : origin = 0xFFD0, length = 0x0002 */
    INT41                   : origin = 0xFFD2, length = 0x0002
    INT42                   : origin = 0xFFD4, length = 0x0002
    INT43                   : origin = 0xFFD6, length = 0x0002
    INT44                   : origin = 0xFFD8, length = 0x0002
    INT45                   : origin = 0xFFDA, length = 0x0002
    INT46                   : origin = 0xFFDC, length = 0x0002
    INT47                   : origin = 0xFFDE, length = 0x0002
    INT48                   : origin = 0xFFE0, length = 0x0002
    INT49                   : origin = 0xFFE2, length = 0x0002
    INT50                   : origin = 0xFFE4, length = 0x0002
    INT51                   : origin = 0xFFE6, length = 0x0002
    INT52                   : origin = 0xFFE8, length = 0x0002
    INT53                   : origin = 0xFFEA, length = 0x0002
    INT54                   : origin = 0xFFEC, length = 0x0002
    INT55                   : origin = 0xFFEE, length = 0x0002
    INT56                   : origin = 0xFFF0, length = 0x0002
    INT57                   : origin = 0xFFF2, length = 0x0002
    INT58                   : origin = 0xFFF4, length = 0x0002
    INT59                   : origin = 0xFFF6, length = 0x0002
    INT60                   : origin = 0xFFF8, length = 0x0002
    INT61                   : origin = 0xFFFA, length = 0x0002
    INT62                   : origin = 0xFFFC, length = 0x0002
    RESET                   : origin = 0xFFFE, length = 0x0002
}

/****************************************************************************/
/* Specify the sections allocation into memory                              */
/****************************************************************************/

SECTIONS
{
    .bss        : {} > RAM                  /* Global & static vars              */
    .data       : {} > RAM                  /* Global & static vars              */
    .TI.noinit  : {} > RAM                  /* For #pragma noinit                */
    .sysmem     : {} > RAM                  /* Dynamic memory allocation area    */
    .stack      : {} > RAM (HIGH)           /* Software system stack             */
    MAIN		: {} > MAIN_CODE

    test_section {
        mTest.obj (.text)
        test.obj (.text)        
        shell.obj (.text)
        test.obj (.const)        
        mTest.obj (.const)
        shell.obj (.const)
        md5.obj (.text)
        uuid.obj (.text)
    } > TEST

    /* meter { */
    /*     meter.obj (.text) */ 
    /*     meter.obj (.const) */
    /* } > METER */

    /* app { */
    /*     app.obj (.text) */
    /*     app.obj (.const) */
    /* } > APP */

    /* nfcProtocol { */
    /*     nfcProtocol.obj (.text) */
    /*     nfcProtocol.obj (.const) */
    /* } > NFC_PROTOCOL */

    /* rtcAlarm { */
    /*     rtcAlarm.obj (.text) */
    /*     rtcAlarm.obj (.const) */
    /* } > RTC_ALARM */

    /* main_text { */
    /*     main.obj (.text:initSystem) */
    /*     main.obj (.text:initPort) */
    /*     main.obj (.text:initClock) */
    /* } > MAIN_TEXT */

    /* dataFlash_text { */
    /*     dataFlash.obj (.text) */
    /*     dataFlash.obj (.const) */
    /* } > DATA_FLASH */

    /* uart_text { */
    /*     uart.obj (.text) */
    /*     uart.obj (.const) */
    /* } > UART */

    /* nfci2c_text { */
    /*     NFC_i2c.obj (.text) */
    /*     NFC_i2c.obj (.const) */
    /* } > NFC_I2C */

    /* flashDriver_text { */
    /*     flashDriver.obj (.text) */
    /*     flashDriver.obj (.const) */
    /* } > FLASH_DRIVER */

    /* lcdDriver_text { */
    /*     lcdDriver.obj (.text) */
    /*     lcdDriver.obj (.const) */
    /* } > LCD_DRIVER */

    /* flashUtil_text { */
    /*     MSP430FlashUtil.obj (.text) */
    /*     MSP430FlashUtil.obj (.const) */
    /* } > FLASH_UTIL */

    /* battery_text { */
    /*     battery.obj (.text) */
    /* } > BATTERY */

    /* meterMisc_text { */
    /*     check_meter_misc.obj (.text) */
    /* } > METER_MISC */

    /* osalTimer { */
    /*     osal_Timer.obj (.text) */
    /*     osal.obj (.text) */
    /*     osal_Timer.obj (.const) */
    /* } > OSAL_TIMER */

    /* taskMgr_text { */
    /*     Task_Mgr.obj (.text) */
    /*     Task_Mgr.obj (.const) */
    /* } > TASK_MGR */

    /* nbiotProcess_text { */
    /*     nbiotProcess.obj (.text) */
    /*     nbiotProcess.obj (.const) */
    /* } > NB_PROCESS */

    /* nbiotResponse_text { */
    /*     nbiotResponse.obj (.text) */
    /*     nbiotResponse.obj (.const) */
    /* } > NB_RESPONSE */

    /* message_text { */
    /*     message.obj (.text) */
    /*     message.obj (.const) */
    /* } > MESSAGE */

    /* modem_text { */
    /*     modem.obj (.text) */
    /*     modem.obj (.const) */
    /* } > MODEM */

    /* test : > FLASHD */
    /* { */
    /*     --library=rts430x_lc_ld_eabi.lib(.text:snprintf) */
    /* } */

    isr_code {
        * (.text:_isr)
        /* app.obj (.text:_isr:Port_1) */
        /* app.obj (.text:_isr:Port_2) */
        /* app.obj (.text:_isr:RTC_ISR) */
        /* app.obj (.text:_isr:UNMI_ISR) */
        /* uart.obj (.text:_isr:USCI_A0_ISR) */
        /* uart.obj (.text:_isr:USCI_A1_ISR) */
        /* uart.obj (.text:_isr:USCI_A2_ISR) */
        /* uart.obj (.text:_isr:USCI_A3_ISR) */
        /* NFC_i2c.obj (.text:_isr:USCI_B3_ISR) */
        /* osal_Timer.obj (.text:_isr:TIMER1_A0_ISR) */
        /* --library=rts430x_lc_ld_eabi.lib(.text:_isr) */
        /* --library=rts430x_lc_ld_eabi.lib(.text:_isr:__TI_ISR_TRAP) */
    } load=0x20000, run=ISR

    library_section : > FLASHB (HIGH)
    {
         --library=rts430x_lc_ld_eabi.lib(.text:_isr:_c_int00_noargs)
    }

    fota_code {
        uart.obj (.text:UART_send)
        uart.obj (.text:UART_receive)
        check_meter_misc.obj (.text:MISC_delayMs)
    } load=FLASHD, run=RAM, table(BINIT)

#ifndef __LARGE_CODE_MODEL__
    .text       : {} > FLASHC       /* Code                              */
#else
    .text       : {} > FLASHC       /* Code                              */
#endif
/* Errata Flash Read Error and Susceptibility for MSP430F54xxA 
Manual placement of interrupt service routines into memory locations above 0x008000
can eliminate the effect on interrupt vector address fetches. */
    .cinit      : {} > FLASHC                /* Initialization tables             */
#ifndef __LARGE_DATA_MODEL__
    .const      : {} > FLASHC                /* Constant data                     */
#else
    .const      : {} >> FLASHC      /* Constant data                     */
#endif
    .cio        : {} > RAM                  /* C I/O Buffer                      */

    .pinit      : {} > FLASHC                /* C++ Constructor tables            */
    .binit      : {} > FLASHC                /* Boot-time Initialization tables   */
    .init_array : {} > FLASHC                /* C++ Constructor tables            */
    .mspabi.exidx : {} > FLASHC              /* C++ Constructor tables            */
    .mspabi.extab : {} > FLASHC              /* C++ Constructor tables            */

    .infoA     : {} > INFOA              /* MSP430 INFO FLASH Memory segments */
    .infoB     : {} > INFOB
    .infoC     : {} > INFOC
    .infoD     : {} > INFOD

    /* MSP430 Interrupt vectors          */
    /* .int00       : {}               > INT00 */
    /* .int01       : {}               > INT01 */
    /* .int02       : {}               > INT02 */
    /* .int03       : {}               > INT03 */
    /* .int04       : {}               > INT04 */
    /* .int05       : {}               > INT05 */
    /* .int06       : {}               > INT06 */
    /* .int07       : {}               > INT07 */
    /* .int08       : {}               > INT08 */
    /* .int09       : {}               > INT09 */
    /* .int10       : {}               > INT10 */
    /* .int11       : {}               > INT11 */
    /* .int12       : {}               > INT12 */
    /* .int13       : {}               > INT13 */
    /* .int14       : {}               > INT14 */
    /* .int15       : {}               > INT15 */
    /* .int16       : {}               > INT16 */
    /* .int17       : {}               > INT17 */
    /* .int18       : {}               > INT18 */
    /* .int19       : {}               > INT19 */
    /* .int20       : {}               > INT20 */
    /* .int21       : {}               > INT21 */
    /* .int22       : {}               > INT22 */
    /* .int23       : {}               > INT23 */
    /* .int24       : {}               > INT24 */
    /* .int25       : {}               > INT25 */
    /* .int26       : {}               > INT26 */
    /* .int27       : {}               > INT27 */
    /* .int28       : {}               > INT28 */
    /* .int29       : {}               > INT29 */
    /* .int30       : {}               > INT30 */
    /* .int31       : {}               > INT31 */
    /* .int32       : {}               > INT32 */
    /* .int33       : {}               > INT33 */
    /* .int34       : {}               > INT34 */
    /* .int35       : {}               > INT35 */
    /* .int36       : {}               > INT36 */
    /* .int37       : {}               > INT37 */
    /* .int38       : {}               > INT38 */
    /* .int39       : {}               > INT39 */
    /* .int40       : {}               > INT40 */
    RTC          : { * ( .int41 ) } > INT41 type = VECT_INIT
    PORT2        : { * ( .int42 ) } > INT42 type = VECT_INIT
    USCI_B3      : { * ( .int43 ) } > INT43 type = VECT_INIT
    USCI_A3      : { * ( .int44 ) } > INT44 type = VECT_INIT
    USCI_B1      : { * ( .int45 ) } > INT45 type = VECT_INIT
    USCI_A1      : { * ( .int46 ) } > INT46 type = VECT_INIT
    PORT1        : { * ( .int47 ) } > INT47 type = VECT_INIT
    TIMER1_A1    : { * ( .int48 ) } > INT48 type = VECT_INIT
    TIMER1_A0    : { * ( .int49 ) } > INT49 type = VECT_INIT
    DMA          : { * ( .int50 ) } > INT50 type = VECT_INIT
    USCI_B2      : { * ( .int51 ) } > INT51 type = VECT_INIT
    USCI_A2      : { * ( .int52 ) } > INT52 type = VECT_INIT
    TIMER0_A1    : { * ( .int53 ) } > INT53 type = VECT_INIT
    TIMER0_A0    : { * ( .int54 ) } > INT54 type = VECT_INIT
    ADC12        : { * ( .int55 ) } > INT55 type = VECT_INIT
    USCI_B0      : { * ( .int56 ) } > INT56 type = VECT_INIT
    USCI_A0      : { * ( .int57 ) } > INT57 type = VECT_INIT
    WDT          : { * ( .int58 ) } > INT58 type = VECT_INIT
    TIMER0_B1    : { * ( .int59 ) } > INT59 type = VECT_INIT
    TIMER0_B0    : { * ( .int60 ) } > INT60 type = VECT_INIT
    UNMI         : { * ( .int61 ) } > INT61 type = VECT_INIT
    SYSNMI       : { * ( .int62 ) } > INT62 type = VECT_INIT
    .reset       : {}               > RESET  /* MSP430 Reset vector         */
}

/****************************************************************************/
/* Include peripherals memory map                                           */
/****************************************************************************/

-l msp430f5419a.cmd


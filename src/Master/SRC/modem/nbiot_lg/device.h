#ifndef __DEVICE_H__
#define __DEVICE_H__

#define DEV_REV_PWRCTRL_MOSFET_ONLY 17
#define DEV_REV_PWRCTRL_LDO_ONLY 18
#define DEV_REV_PWRCTRL_MOSFET_LDO 19

#define DEVICE_REVISION DEV_REV_PWRCTRL_MOSFET_LDO

#define NBIOT_DEVICE 1
#define LORA_DEVICE 0

/**
 * @brief Define firmware main version. It depends on the product family.
 */
// #if defined(NBIOT_MODEM_TPB23)
// #if defined(AUX_REPEATER)
// #define FW_VER_MAIN "6"
// #define FIRMWARE_VER "U" FW_VER_MAIN "19" //TODO
// #else
// #define FW_VER_MAIN "2"
// #define FIRMWARE_VER "U" FW_VER_MAIN "19" //TODO
// #endif
#if defined(NBIOT_MODEM_BC95G)
#if defined(AUX_REPEATER)
#define FW_VER_MAIN "7"
#define FIRMWARE_VER "U" FW_VER_MAIN "01"
#else
#define FW_VER_MAIN "3"
#define FIRMWARE_VER "U" FW_VER_MAIN "26"
#endif
#else
#error "NB-IoT Modem model is not defined. please define V120 or V150."
#endif

#define FIRMWARE_VER_LEN 4

#define LCD_THROUGH_UART 1

#if LCD_THROUGH_UART
#define LCD_THROUGH_GPIO 0
#else
#define LCD_THROUGH_GPIO 1
#endif

/**
 * @brief Function definition. If defined, Use LG U+ platform.
 */
// #define MODEM_FUN_EN_PLATFORM

/**
 * @brief Function definition. If defined, Use PSM instead of power off
 */
// #define MODEM_FUN_EN_PSM

/*********************************/
/********** 데이터 갯수 **********/
/*********************************/

// 메모리에 갖고 있는 데이터 수
#define MAX_NUM_STORED_DATA 24

// 메시지에 실릴 데이터 갯수
#define NUM_NBIOT_STORED_DATA 24

#endif

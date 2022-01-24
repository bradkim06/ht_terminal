#ifndef __DEVICE_H__
#define __DEVICE_H__

#define LORA_DEVICE 1
#define NBIOT_DEVICE 0

#define FIRMWARE_VER "A002"
#define FIRMWARE_VER_LEN 4

// LoRa 단말의 LCD는 UART를 통해 접속됨 (경우에 따라 GPIO로 동작할 수 있음.)
#define LCD_THROUGH_UART 1
#if LCD_THROUGH_UART
#define LCD_THROUGH_GPIO 0
#else
#define LCD_THROUGH_GPIO 1
#endif

/*********************************/
/********** 데이터 갯수 **********/
/*********************************/

// 메모리에 갖고 있는 데이터 수
#define MAX_NUM_STORED_DATA 12

// 메시지에 실릴 데이터 갯수
#define NUM_LORA_STORED_DATA MAX_NUM_STORED_DATA

#endif

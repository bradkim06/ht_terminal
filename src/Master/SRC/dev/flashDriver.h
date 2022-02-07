#ifndef __FLASH_DRIVER_H__
#define __FLASH_DRIVER_H__

#include "common_header.h"

//! INFO flash sector count
#define NUM_FLASH_SECTOR 4

//! INFO flash sector length
#define LEN_INFO_FLASH_SECTOR 128

//! Configuration data length without checksum
#define LEN_FLASH_CONFIG 31

#if LORA_DEVICE
typedef struct {
	uint8 meterType; // 0
	uint8 sleepMode; // 1
	uint8 meterInterval; // 2
	uint8 reportInterval; // 3
	uint8 unused; // 4
	uint8 isShortInterval; // 5
	uint8 resetCount[2]; // 6-7
	uint8 debugPrint; // 8
	uint8 riCtrlMode; // 9
	uint8 periodMode; // 10
	uint8 reportRange; // 11
	uint8 reserved[19]; // 12 ~ 30
	uint8 checksum; // 31
} FlashConfig_t;

#else // NBIOT_DEVICE
typedef struct {
	uint8 meterType; // 0
	uint8 sleepMode; // 1
	uint8 meterInterval; // 2
	uint8 reportInterval; // 3
	uint8 serverIp[4]; // 4, 5, 6, 7
	uint8 serverPort[2]; // 8, 9
	uint8 resetCount[2]; // 10, 11
	uint8 isShortInterval; // 12
	uint8 debugPrint; // 13
	// service code는 현재 LG플랫폼 연동시에만 유효
	uint8 serviceCode[4]; // 14, 15, 16, 17
	uint8 isModemInit; // 18
	uint8 fotaIp[4]; // 19, 20, 21, 22
	uint8 fotaPort[2]; // 23, 24
	uint8 fotaInterval; // 25
	uint8 riCtrlMode; // 26
	uint8 periodMode; // 27
	uint8 reportRange; // 28
	uint8 reserved[2]; // 29 ~ 30
	uint8 checksum; // 31
} FlashConfig_t;

#endif

#define LEN_FLASH_ID 31
#if LORA_DEVICE
typedef struct {
	uint8 serialNum[SERIAL_NUM_LEN]; //  0 ~ 11
	uint8 devEui[8]; // 12 ~ 19
#if defined(AUX_REPEATER)
	// 주의 - pan/nwk는 구조체의 짝수 번째 위치(아래의 20, 22와 같이)에서 시작해야 함.
	uint16 pan_id; // 20 ~ 21
	uint32 nwk_addr; // 22 ~ 25
	uint8 slaveId; // 26
	uint8 reserved[4]; // 27 ~ 30
#else
	uint8 reserved[11]; // 20 ~ 30
#endif
	uint8 checksum; // 31
} FlashId_t;
#else
typedef struct {
	uint8 serialNum[SERIAL_NUM_LEN]; //  0 ~ 11
	uint8 imei[8]; // 12 ~ 19
#if defined(AUX_REPEATER)
	// 주의 - pan/nwk는 구조체의 짝수 번째 위치에서 시작해야 함.
	uint16 pan_id; // 20 ~ 21
	uint32 nwk_addr; // 22 ~ 25
	uint8 slaveId; // 26
	uint8 reserved[4]; // 27 ~ 30
#else
	uint8 reserved[11]; // 20 ~ 30
#endif
	uint8 checksum; // 31
} FlashId_t;
#endif

/**************************************************
 * Flash R/W/E interface for Configuration data
 **************************************************/
void FLASH_saveConfigInfo(Config_t *config);
void FLASH_readConfigInfo(Config_t *config);
uint8 FLASH_readResetCause();
void FLASH_updateResetCount(Config_t *config);

/**************************************************
 * Etc supporting functions
 **************************************************/
void bcd2int(uint8 *pBCD, uint32 *pInt, uint8 nDigit);
void int2bcd(uint32 *pInt, uint8 *pBCD, uint8 nDigit);
uint8 ascii2Hex(char ch);
uint8 ascii2BCD(char a, char b);
void ip2hexArray(char *ip, uint8 *hexIp);
#if defined(AUX_REPEATER)
int getFirstZeroPosition(uint32 nwk_addr);
#endif
void distributingReportTime(int serialBase, Config_t *config);

#endif //__FLASH_DRIVER_H__

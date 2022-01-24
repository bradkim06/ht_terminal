#include <msp430.h>
#include "common_header.h"
#include "check_meter_misc.h"
#include "MSP430FlashUtil.h"
#include "flashDriver.h"
#include "app.h"
#include "meter.h"
#include "rtcAlarm.h"
#include "osal.h"

const uint8 *FlashAddr[NUM_FLASH_SECTOR] = { (uint8 *)FLASH_SEGA_ADDR, (uint8 *)FLASH_SEGB_ADDR,
					     (uint8 *)FLASH_SEGC_ADDR, (uint8 *)FLASH_SEGD_ADDR };

static uint8 calChecksum(uint8 *p, int len)
{
	uint8 checksum = 0;
	for (int i = 0; i < len; i++) {
		checksum += *p++;
	}

	return checksum;
}

/**************************************************
 * Flash functions for Configuration data
 **************************************************/

uint8 FLASH_readResetCause()
{
	uint8 cause = 0;

	halIntState_t intState;
	HAL_ENTER_CRITICAL_SECTION(intState);
	MSP430FLASH_read((uint8 *)SYSRSTIV_ADDR, &cause, 1);
	// 현재 Reset cuase data를 사용하고 있지 않으므로 추가처리는 생략한다.
	MSP430FLASH_erasePage((uint8 *)SYSRSTIV_ADDR);
	HAL_EXIT_CRITICAL_SECTION(intState);

	return cause;
}

static void saveConfig(FlashConfig_t *pInfo)
{
	// Info Flash A와 C에 동일한 정보를 저장함

	uint8 flash[LEN_INFO_FLASH_SECTOR];

	halIntState_t intState;
	HAL_ENTER_CRITICAL_SECTION(intState);

	BOOL result = FALSE;
	MSP430FLASH_read((uint8 *)FLASH_SEGA_ADDR, flash, LEN_INFO_FLASH_SECTOR);
	memcpy(flash, (uint8 *)pInfo, sizeof(FlashConfig_t));

	if (MSP430FLASH_erasePage((uint8 *)FLASH_SEGA_ADDR)) {
		if (MSP430FLASH_write((uint8 *)FLASH_SEGA_ADDR, flash, LEN_INFO_FLASH_SECTOR)) {
			result = TRUE;
		}
	}

	MSP430FLASH_read((uint8 *)FLASH_SEGC_ADDR, flash, LEN_INFO_FLASH_SECTOR);
	memcpy(flash, (uint8 *)pInfo, sizeof(FlashConfig_t));

	if (MSP430FLASH_erasePage((uint8 *)FLASH_SEGC_ADDR)) {
		if (MSP430FLASH_write((uint8 *)FLASH_SEGC_ADDR, flash, LEN_INFO_FLASH_SECTOR)) {
			result = TRUE;
		}
	}

	HAL_EXIT_CRITICAL_SECTION(intState);
	if (!result) {
		OSAL_setEvent(AppTaskId, APP_EVENT_REBOOT);
	}
}

#if NBIOT_DEVICE
#include "modem.h"
#endif
static void readConfig(FlashConfig_t *pInfo)
{
	// Info Flash A와 C에 저장된 정보를 읽어서 checksum을 확인하여
	// 저장된 값의 에러 여부를 확인함. 만일 두 부분 중 한 곳의 정보가
	// 깨져 있다면 에러 없는 부분의 데이터를 복사하여 복구함.

	uint8 flash[LEN_INFO_FLASH_SECTOR];

	halIntState_t intState;

	HAL_ENTER_CRITICAL_SECTION(intState);
	MSP430FLASH_read((uint8 *)FLASH_SEGA_ADDR, flash, LEN_INFO_FLASH_SECTOR);
	HAL_EXIT_CRITICAL_SECTION(intState);

	FlashConfig_t info_a;
	memcpy((uint8 *)&info_a, flash, sizeof(FlashConfig_t));

	uint8 checksum = calChecksum((uint8 *)&info_a, LEN_FLASH_CONFIG);
	BOOL a_valid = (checksum == info_a.checksum) ? TRUE : FALSE;

	HAL_ENTER_CRITICAL_SECTION(intState);
	MSP430FLASH_read((uint8 *)FLASH_SEGC_ADDR, flash, LEN_INFO_FLASH_SECTOR);
	HAL_EXIT_CRITICAL_SECTION(intState);

	FlashConfig_t info_c;
	memcpy((uint8 *)&info_c, flash, sizeof(FlashConfig_t));
	checksum = calChecksum((uint8 *)&info_c, LEN_FLASH_CONFIG);
	BOOL c_valid = (checksum == info_c.checksum) ? TRUE : FALSE;

	if (a_valid == TRUE && c_valid == TRUE) {
		memcpy((uint8 *)pInfo, (uint8 *)&info_a, sizeof(FlashConfig_t));
	} else if (a_valid == TRUE && c_valid == FALSE) {
		memcpy((uint8 *)pInfo, (uint8 *)&info_a, sizeof(FlashConfig_t));
		saveConfig(pInfo);
	} else if (a_valid == FALSE && c_valid == TRUE) {
		memcpy((uint8 *)pInfo, (uint8 *)&info_c, sizeof(FlashConfig_t));
		saveConfig(pInfo);
	} else {
		printf("Config: both side are false\n");
		memcpy((uint8 *)pInfo, (uint8 *)&info_a, sizeof(FlashConfig_t));
		pInfo->meterType = W_STANDARD_D;
		pInfo->meterInterval = 1;
		pInfo->reportInterval = 6;
		pInfo->reportRange = pInfo->reportInterval; // default
		pInfo->sleepMode = 0;
		pInfo->riCtrlMode = 0;
		pInfo->periodMode = 0;
		pInfo->isShortInterval = 0;
		pInfo->debugPrint = 0;

#if NBIOT_DEVICE
		ip2hexArray((char *)LG_DEFAULT_FOTA_SERVER_IP, pInfo->fotaIp);
		pInfo->fotaPort[0] = LO_UINT16(LG_DEFAULT_FOTA_SERVER_PORT);
		pInfo->fotaPort[1] = HI_UINT16(LG_DEFAULT_FOTA_SERVER_PORT);
		pInfo->fotaInterval = LG_DEFAULT_FOTA_DAY_INTERVAL;
#endif

		memset(pInfo->resetCount, 0, 2);
		saveConfig(pInfo);
	}
}

static void saveID(FlashId_t *pInfo)
{
	// Info Flash B와 D에 동일한 정보를 저장함

	uint8 flash[LEN_INFO_FLASH_SECTOR];

	halIntState_t intState;
	HAL_ENTER_CRITICAL_SECTION(intState);

	BOOL result = FALSE;
	MSP430FLASH_read((uint8 *)FLASH_SEGB_ADDR, flash, LEN_INFO_FLASH_SECTOR);
	memcpy(flash, (uint8 *)pInfo, sizeof(FlashId_t));

	if (MSP430FLASH_erasePage((uint8 *)FLASH_SEGB_ADDR)) {
		if (MSP430FLASH_write((uint8 *)FLASH_SEGB_ADDR, flash, LEN_INFO_FLASH_SECTOR)) {
			result = TRUE;
		}
	}

	MSP430FLASH_read((uint8 *)FLASH_SEGD_ADDR, flash, LEN_INFO_FLASH_SECTOR);
	memcpy(flash, (uint8 *)pInfo, sizeof(FlashId_t));
	if (MSP430FLASH_erasePage((uint8 *)FLASH_SEGD_ADDR)) {
		if (MSP430FLASH_write((uint8 *)FLASH_SEGD_ADDR, flash, LEN_INFO_FLASH_SECTOR)) {
			result = TRUE;
		}
	}

	HAL_EXIT_CRITICAL_SECTION(intState);
	if (!result) {
		OSAL_setEvent(AppTaskId, APP_EVENT_REBOOT);
	}
}

static void readID(FlashId_t *pInfo)
{
	// Info Flash B와 D에 저장된 정보를 읽어서 checksum을 확인하여
	// 저장된 값의 에러 여부를 확인함. 만일 두 부분 중 한 곳의 정보가
	// 깨져 있다면 에러 없는 부분의 데이터를 복사하여 복구함.

	uint8 flash[LEN_INFO_FLASH_SECTOR];

	halIntState_t intState;

	HAL_ENTER_CRITICAL_SECTION(intState);
	MSP430FLASH_read((uint8 *)FLASH_SEGB_ADDR, flash, LEN_INFO_FLASH_SECTOR);
	HAL_EXIT_CRITICAL_SECTION(intState);

	FlashId_t info_b;
	memcpy((uint8 *)&info_b, flash, sizeof(FlashId_t));

	uint8 checksum = calChecksum((uint8 *)&info_b, LEN_FLASH_ID);
	BOOL b_valid = (checksum == info_b.checksum) ? TRUE : FALSE;

	HAL_ENTER_CRITICAL_SECTION(intState);
	MSP430FLASH_read((uint8 *)FLASH_SEGD_ADDR, flash, LEN_INFO_FLASH_SECTOR);
	HAL_EXIT_CRITICAL_SECTION(intState);

	FlashId_t info_d;
	memcpy((uint8 *)&info_d, flash, sizeof(FlashId_t));

	checksum = calChecksum((uint8 *)&info_d, LEN_FLASH_ID);
	BOOL d_valid = (checksum == info_d.checksum) ? TRUE : FALSE;

	if (b_valid == TRUE && d_valid == TRUE) {
		memcpy((uint8 *)pInfo, (uint8 *)&info_b, sizeof(FlashId_t));
	} else if (b_valid == TRUE && d_valid == FALSE) {
		memcpy((uint8 *)pInfo, (uint8 *)&info_b, sizeof(FlashId_t));
		saveID(pInfo);
	} else if (b_valid == FALSE && d_valid == TRUE) {
		memcpy((uint8 *)pInfo, (uint8 *)&info_d, sizeof(FlashId_t));
		saveID(pInfo);
	} else {
		memcpy((uint8 *)pInfo, (uint8 *)&info_b, sizeof(FlashId_t));
		printf("ID: both side are false\n");
		// 두개의 영역이 모두 깨지더라도 Serial Number는 지우지 않음.
#if 0
        memset((uint8 *)pInfo, 0xff, sizeof(FlashId_t));
        memset(pInfo->serialNum, '0', SERIAL_NUM_LEN);

        saveID(pInfo);
#endif
	}
}

void bcd2int(uint8 *pBCD, uint32 *pInt, uint8 nDigit)
{
	uint32 value = 0;
	uint8 temp = 0;

	for (int i = 0; i < nDigit; i++) {
		value *= 100;
		temp = *(pBCD + i);
		temp = ((temp & 0xf0) >> 4) * 10 + (temp & 0x0f);
		value += temp;
	}

	*pInt = value;
}

void int2bcd(uint32 *pInt, uint8 *pBCD, uint8 nDigit)
{
	uint32 value = *pInt;
	uint8 temp = 0;

	for (int i = 0; i < nDigit; i++) {
		temp = value % 100;
		value /= 100;
		*(pBCD + (nDigit - 1 - i)) = (temp / 10) * 0x10 + (temp % 10);
	}
}

#if defined(AUX_REPEATER)
int getFirstZeroPosition(uint32 nwk_addr)
{
	uint8 temp[4];
	int firstZero = 0;
	memcpy(temp, &nwk_addr, 4);

	for (int i = 0; i < 4; i++) {
		if (temp[i]) {
			break;
		}
		firstZero = i;
	}
	return firstZero;
}
#endif

void FLASH_saveConfigInfo(Config_t *config)
{
	// save serial number
	FlashId_t flashID;

#if defined(AUX_REPEATER)
	memcpy(&flashID.pan_id, &config->pan_id, 2);
	memcpy(&flashID.nwk_addr, &config->nwk_addr, 4);

	int zeroPos = getFirstZeroPosition(config->nwk_addr);
	uint8 addr[4];
	memcpy(addr, &config->slaveNwk, 4);
	flashID.slaveId = addr[zeroPos];
#endif

	memcpy(flashID.serialNum, config->serialNum, SERIAL_NUM_LEN);
#if LORA_DEVICE
	memcpy(flashID.devEui, config->devEui, 8);
#else // NBIOT_DEVICE
	memcpy(flashID.imei, config->imei, 8);
#endif

	flashID.checksum = calChecksum((uint8 *)&flashID, LEN_FLASH_ID);

	saveID(&flashID);

	// save others
	FlashConfig_t flashConfig;

	flashConfig.meterType = config->meterType;
	flashConfig.sleepMode = config->sleepMode;
	flashConfig.riCtrlMode = config->riCtrlMode;
	flashConfig.periodMode = config->periodMode;
	flashConfig.meterInterval = config->meterInterval;
	flashConfig.reportInterval = config->reportInterval;
	flashConfig.reportRange = config->reportRange;
#if NBIOT_DEVICE
	ip2hexArray(config->serverIp, flashConfig.serverIp);
	memcpy(flashConfig.serverPort, &config->serverPort, 2);
	ip2hexArray(config->fotaIp, flashConfig.fotaIp);
	memcpy(flashConfig.fotaPort, &config->fotaPort, 2);
	flashConfig.fotaInterval = config->fotaInterval;
	memcpy(flashConfig.serviceCode, &config->serviceCode, 4);
	flashConfig.isModemInit = config->isModemInit;
#endif
	flashConfig.isShortInterval = config->isShortInterval;
	flashConfig.debugPrint = config->debugPrint;

	memcpy(flashConfig.resetCount, &config->resetCount, 2);

	Date_t date;
	RTC_read(&date);

	flashConfig.checksum = calChecksum((uint8 *)&flashConfig, LEN_FLASH_CONFIG);
	saveConfig(&flashConfig);
}

void FLASH_readConfigInfo(Config_t *config)
{
	config->termModel = MISC_getDeviceType();
	config->bslModel = MISC_getBslType();

	// read others
	FlashConfig_t flashConfig;
	readConfig(&flashConfig);

	config->meterType = flashConfig.meterType;
	config->meterInterval = flashConfig.meterInterval;
	config->reportInterval = flashConfig.reportInterval;
	config->reportRange = flashConfig.reportRange;

#if NBIOT_DEVICE
	sprintf(config->serverIp, "%d.%d.%d.%d", flashConfig.serverIp[0], flashConfig.serverIp[1],
		flashConfig.serverIp[2], flashConfig.serverIp[3]);
	memcpy(&config->serverPort, flashConfig.serverPort, 2);
	sprintf(config->fotaIp, "%d.%d.%d.%d", flashConfig.fotaIp[0], flashConfig.fotaIp[1],
		flashConfig.fotaIp[2], flashConfig.fotaIp[3]);
	memcpy(&config->fotaPort, flashConfig.fotaPort, 2);
	config->fotaInterval = flashConfig.fotaInterval;
	memcpy(config->serviceCode, flashConfig.serviceCode, 4);
	config->serviceCode[4] = 0;
	config->isModemInit = flashConfig.isModemInit;
#endif
	config->sleepMode = flashConfig.sleepMode;
	config->riCtrlMode = flashConfig.riCtrlMode;
	config->periodMode = flashConfig.periodMode;
	config->isShortInterval = flashConfig.isShortInterval;
	config->debugPrint = flashConfig.debugPrint;

	memcpy(&config->resetCount, flashConfig.resetCount, 2);

	// read serial number
	FlashId_t flashID;
	readID(&flashID);

#if defined(AUX_REPEATER)
	memcpy(&config->pan_id, &flashID.pan_id, 2);
	memcpy(&config->nwk_addr, &flashID.nwk_addr, 4);

	int zeroPos = getFirstZeroPosition(flashID.nwk_addr);
	uint8 addr[4];
	memcpy(addr, &flashID.nwk_addr, 4);
	addr[zeroPos] = flashID.slaveId;
	memcpy(&config->slaveNwk, addr, 4);
#endif

#if LORA_DEVICE
	memcpy(config->devEui, flashID.devEui, 8);
#else // NBIOT_DEVICE
	memcpy(config->imei, flashID.imei, 8);
#endif

	memcpy(config->serialNum, flashID.serialNum, SERIAL_NUM_LEN);

	int serialBase = ascii2Hex(flashID.serialNum[8]) * 1000 +
			 ascii2Hex(flashID.serialNum[9]) * 100 +
			 ascii2Hex(flashID.serialNum[10]) * 10 + ascii2Hex(flashID.serialNum[11]);

#if 1 // sholee
	int nSpread = config->reportRange * 50;
#else
	int nSpread = config->reportInterval * 50;
#endif

	config->reportSec = ((serialBase / nSpread) % 4) *
			    15; // 같은 '분'에 보고하는 단말기들간에 시간을 15초 단위로 4개로 나눔
	serialBase %= nSpread;

	config->intervalBaseTime = serialBase / 50;
	config->reportMin = (serialBase % 50) + 5;
}

uint8 ascii2Hex(char ch)
{
	uint8 hex = 0;

	if (ch >= 'A' && ch <= 'F') {
		hex = (ch - 'A') + 10;
	} else if (ch >= 'a' & ch <= 'f') {
		hex = (ch - 'a') + 10;
	} else if (ch >= '0' & ch <= '9') {
		hex = ch - '0';
	}
	return hex;
}

uint8 ascii2BCD(char a, char b)
{
	return (ascii2Hex(a) * 0x10 + ascii2Hex(b));
}

void ip2hexArray(char *ip, uint8 *hexIp)
{
	char *p = ip;

	*(hexIp + 0) = atoi(p);

	int valid = 1;
	for (int i = 1; i < 4; i++) {
		if ((p = strstr(p, ".")) == NULL) {
			valid = 0;
			break;
		}
		p += 1;

		*(hexIp + i) = atoi(p);
	}

	if (valid == 0) {
		memset(hexIp, 0, 4);
	}
}

void FLASH_updateResetCount(Config_t *config)
{
	if (config->resetCount < 65000) {
		config->resetCount++;
		FLASH_saveConfigInfo(config);
	}
}

#ifndef __DATA_FLASH_H__
#define __DATA_FLASH_H__

#define DATA_FLASH_TOP_ADDR 0x23400 // 10KB(0x23400 ~ 0x25BFF)

#define SECTOR_SIZE 512

// data sector
#define FIRST_DATA_SECTOR 0
#define NUM_DATA_SECTOR 18
#define UNIT_PER_DATA_SECTOR 5 // 하나의 sector는 다섯 조각으로 구성
#define NUM_DATE_TO_STORE (NUM_DATA_SECTOR * UNIT_PER_DATA_SECTOR)

#define FLASH_DATA_MARKER 0x41544144

// map sector
#define MAP_SECTOR (FIRST_DATA_SECTOR + NUM_DATA_SECTOR)
#define NUM_MAP_SECTOR 1
#define FLASH_MAP_MARKER 0x2050414D

#define DATA_FLASH_RETRY_COUNT 5

#define ALL_FF 0xFFFFFFFF

typedef struct { // 100 bytes
	uint8 year;
	uint8 mon;
	uint8 day;
	uint8 caliber_dp;
	uint8 data[24][4]; // 96
} data_unit_t;

typedef struct { // 508
	uint32 marker; // 4
	data_unit_t dataUnit[UNIT_PER_DATA_SECTOR]; // 500
	uint32 checksum; // 4
} data_sector_t;

typedef struct { // 4
	uint8 year;
	uint8 mon;
	uint8 day;
	uint8 savePos;
} unit_map_t;

typedef struct { // 368
	uint32 marker; // 4
	unit_map_t unitMap[NUM_DATE_TO_STORE]; // 90 * 4 = 360
	uint32 checksum; // 4
} map_sector_t;

typedef struct {
	uint8 year;
	uint8 mon;
	uint8 day;
	uint8 hour;
	uint8 caliber_dp;
	uint8 meterValue[4];
} flash_save_data_t;

typedef struct {
	uint8 year;
	uint8 mon;
	uint8 dayFlag[4];
} unit_day_data_map_t;

typedef struct {
	uint8 nDays;
	unit_day_data_map_t unitDayDataMap[5];
} day_data_map_t;

typedef struct {
	uint8 year;
	uint8 mon;
	uint8 day;
} ymd_t;

typedef struct {
	uint8 year;
	uint8 mon;
	uint8 day;
	uint8 savePos;
	uint16 sortKey;
	uint8 txSeq;
} date_sort_t;

void dataFlash_init();
void dataFlash_eraseSector(char *str);
void dataFlash_displayDataSector(int sector);
void dataFlash_displayMapSector();
int dataFlash_getList(uint8 *pList);
void dataFlash_readData(int yearFrom, int monFrom, int dayFrom, int nDay);
void dataFlash_genData(int count, int skip);
void dataFlash_save(uint8 *pNewData, uint8 caliber_dp);
int dataFlash_sendData(uint8 yearFrom, uint8 monFrom, uint8 dayFrom, uint8 yearTo, uint8 monTo,
		       uint8 dayTo);
#endif

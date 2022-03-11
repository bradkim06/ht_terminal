
//MSP430I2C.h

#ifndef __NFC_I2C_H__
#define __NFC_I2C_H__

//#define NTAG_2k
#define NTAG_1k

#if !defined(MIN)
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define NTAG_MAX_WRITE_DELAY_MS 10

#define NTAG_I2C_BLOCK_SIZE 0x10

//----------------------------------------------------------------------
///
/// memory addresses as seen from i2c interface
#define NTAG_MEM_ADRR_I2C_ADDRESS 0x00
#define NTAG_MEM_BLOCK_START_USER_MEMORY 0x01
#define NTAG_MEM_ADDR_START_USER_MEMORY NTAG_MEM_BLOCK_START_USER_MEMORY *NTAG_I2C_BLOCK_SIZE

#define NTAG_MEM_BLOCK_CONFIGURATION_2k 0x7A
#define NTAG_MEM_ADDR_CONFIGURATION_2k NTAG_MEM_BLOCK_CONFIGURATION_2k *NTAG_I2C_BLOCK_SIZE
#define NTAG_MEM_BLOCK_CONFIGURATION_1k 0x3A
#define NTAG_MEM_ADDR_CONFIGURATION_1k NTAG_MEM_BLOCK_CONFIGURATION_1k *NTAG_I2C_BLOCK_SIZE

#ifdef NTAG_2k
#define NTAG_MEM_BLOCK_CONFIGURATION_ADDR NTAG_MEM_BLOCK_CONFIGURATION_2k
#elif defined(NTAG_1k)
#define NTAG_MEM_BLOCK_CONFIGURATION_ADDR NTAG_MEM_BLOCK_CONFIGURATION_1k
#endif

#define NTAG_MEM_AUTH_MEMORY_CONFIGURATION 0x38
#define NTAG_MEM_PASSWORD_ACCESS_CONFIGURATION 0x39

#define NTAG_MEM_BLOCK_SESSION_REGS 0xFE

#define NTAG_MEM_BLOCK_START_SRAM 0xF8
#define NTAG_MEM_ADDR_START_SRAM NTAG_MEM_BLOCK_START_SRAM *NTAG_I2C_BLOCK_SIZE

#define NTAG_MEM_SRAM_BLOCKS 4
#define NTAG_MEM_SRAM_SIZE NTAG_MEM_SRAM_BLOCKS *NTAG_I2C_BLOCK_SIZE

//----------------------------------------------------------------------
///
///  byte offset in session and configuration
#define NTAG_MEM_OFFSET_NC_REG 0x00
#define NTAG_MEM_OFFSET_LAST_NDEF_BLOCK 0x01
#define NTAG_MEM_OFFSET_SRAM_MIRROR_BLOCK 0x02
#define NTAG_MEM_OFFSET_WDT_LS 0x03
#define NTAG_MEM_OFFSET_WDT_MS 0x04
#define NTAG_MEM_OFFSET_I2C_CLOCK_STR 0x05
#define NTAG_MEM_OFFSET_REG_LOCK 0x06
#define NTAG_MEM_OFFSET_NS_REG 0x07

//----------------------------------------------------------------------
///
///  memory bit masks
#define NTAG_NC_REG_MASK_I2C_RST_ON_OFF 0x80
#define NTAG_NC_REG_MASK_PTHRU_ON_OFF 0x40
#define NTAG_NC_REG_MASK_FD_OFF 0x30
#define NTAG_NC_REG_MASK_FD_ON 0x0C
#define NTAG_NC_REG_MASK_SRAM_MIRROR_ON_OFF 0x02
#define NTAG_NC_REG_MASK_TRANSFER_DIR 0x01

#define NTAG_REG_LOCK_MASK_CONF_BYTES_ACCESS_I2C 0x02
#define NTAG_REG_LOCK_MASK_CONF_BYTES_ACCESS_RF 0x01

#define NTAG_NS_REG_MASK_NDEF_DATA_READ 0x80
#define NTAG_NS_REG_MASK_I2C_LOCKED 0x40
#define NTAG_NS_REG_MASK_RF_LOCKED 0x20
#define NTAG_NS_REG_MASK_SRAM_I2C_READY 0x10
#define NTAG_NS_REG_MASK_SRAM_RF_READY 0x08
#define NTAG_NS_REG_MASK_EEPROM_WR_ERR 0x04
#define NTAG_NS_REG_MASK_EEPROM_WR_BUSY 0x02
#define NTAG_NS_REG_MASK_RF_FIELD_PRESENT 0x01

//----------------------------------------------------------------------
//PASSWORD_ACCESS_CONFIGURATION
//----------------------------------------------------------------------
#define NTAG_PA_CONFIG_ACCESS 0x00
#define NTAG_PA_CONFIG_PWD 0x04 //0x04 ~ 0x07
#define NTAG_PA_CONFIG_PACK 0x08 //0x08 ~ 0x09
#define NTAG_PA_CONFIG_PT_I2C 0x0C

#define NTAG_ACCESS_NFC_PROT (0x1 << 7)

#define NTAG_PT_I2C_MASK_SRAM_PROT (0x1 << 2)
#define NTAG_PT_I2C_MASK_I2C_PROT 0x3

//
#define NTAG_AUTH_CONFIG_AUTH0 0x0F

//----------------------------------------------------------------------
///
/// error codes

#define NTAG_RET_OK 0x00
#define NTAG_RET_ERR 0x01

//#define NTAG_ERR_OK                                    0x00
//#define NTAG_ERR_COMMUNICATION                        -0x01
//#define NTAG_ERR_BUFF_OVERFLOW                        -0x02
//#define NTAG_ERR_INIT_FAILED                          -0x03
//#define NTAG_ERR_INVALID_PARAM                        -0x09

#define SRAM_TIMEOUT 500 //msec

#define NFC_NORMAL_MODE 0x00
#define NFC_FACTORY_RESET 0x01

/***********************************************************************/
/* TYPES                                                               */
/***********************************************************************/
typedef enum {
	RF_SWITCHED_OFF_00b = (0x0 << 4),
	HALT_OR_RF_SWITCHED_OFF_01b = (0x1 << 4),
	LAST_NDEF_BLOCK_READ_OR_RF_SWITCHED_OFF_10b = (0x2 << 4),
	I2C_LAST_DATA_READ_OR_WRITTEN_OR_RF_SWITCHED_OFF_11b = (0x3 << 4)
} NtagFdOffFunctions_t;

typedef enum {
	RF_SWITCHED_ON_00b = (0x0 << 2),
	FIRST_VALID_SoF_01b = (0x1 << 2),
	SELECTION_OF_TAG_10b = (0x2 << 2),
	DATA_READY_BY_I2C_OR_DATA_READ_BY_RF_11b = (0x3 << 2)
} NtagFdOnFunctions_t;

typedef enum { RF_TO_I2C = NTAG_NC_REG_MASK_TRANSFER_DIR, I2C_TO_RF = 0 } NtagTransferDir_t;

// BOOL NTAG_writeSramData(uint8 *bytes, uint16 len);
// BOOL NTAG_readSramData(uint8 *bytes, uint16 len);

// BOOL NTAG_setI2CRstOnOff(BOOL on);
// BOOL NTAG_getI2CRstOnOff(BOOL *on);

// BOOL NTAG_setRFConfigurationWrite();
// BOOL NTAG_getRFConfigurationLock( BOOL *locked);

// BOOL NTAG_setI2CConfigurationWrite();
// BOOL NTAG_getI2CConfigurationLock( BOOL *locked);

// BOOL NTAG_getI2CClockStr( BOOL *clk);
// BOOL NTAG_ReleaseI2CLocked();

// BOOL NTAG_setFDOnFunction( uint8 func);
// BOOL NTAG_getFDOnFunction( uint8 *func);

// BOOL NTAG_setFDOffFunction( uint8 func);
// BOOL NTAG_getFDOffFunction( uint8 *func);

// BOOL NTAG_setPthruOnOff( BOOL on);
// BOOL NTAG_getPthruOnOff( BOOL *on);

// BOOL NTAG_setSRAMMirrorOnOff( BOOL on);
// BOOL NTAG_getSRAMMirrorOnOff( BOOL *on);

// BOOL NTAG_setTransferDir( uint8 dir);
// BOOL NTAG_getTransferDir(uint8 *dir);

// BOOL NTAG_setLastNDEFBlock(uint8 block);
// BOOL NTAG_getLastNDEFBlock(uint8 *block);

// BOOL NTAG_setSRAMMirrorBlock(uint8 block);
// BOOL NTAG_getSRAMMirrorBlock(uint8 *block);

// BOOL NTAG_setWatchdogTime(uint16 time);
// BOOL NTAG_getWatchdogTime(uint16 *time);

void NFC_init();
void NFC_tagEnable();
void NFC_tagDisable();
void NFC_fdEnable();
void NFC_fdDisable();
BOOL NFC_factoryResetTag();
BOOL NFC_checkRead(int msec);

BOOL NFC_sendMessage(byte *msg, uint8 len);
BOOL NFC_recvMessage();
BOOL NFC_tagDetect();

#endif

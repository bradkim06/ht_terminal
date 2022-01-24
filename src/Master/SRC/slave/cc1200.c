#include "msp430.h"
#include "common_header.h"
#include "hal_spi_rf_trxeb.h"
#include "cc120x_spi.h"
#include "bsp.h"
#include "cc1200.h"
#include "CC1200_reg_config.h"
#include "osal_Timer.h"
#include "check_meter_misc.h"
#include "slaveAccess.h"
#include "rtcAlarm.h"
#include "crc.h"
#include "port_desc.h"
#include "uart.h"
#include "test.h"

#define MAX_DEPTH 4
uint32 MaskAddr[MAX_DEPTH + 1] = { 0x00000000, 0xFF000000, 0xFFFF0000, 0xFFFFFF00, 0xFFFFFFFF };

extern Config_t conf;

// Test mode가 아니며, DEBUG 동작 중인 경우에만 Print.
#if defined(DEBUG)
#define CC1200_DBG(...)                                                                            \
	do {                                                                                       \
		if (TEST_isTestMode() == FALSE) {                                                  \
			printf_ts("[CC1200]");                                                     \
			printf(__VA_ARGS__);                                                       \
		}                                                                                  \
	} while (0);
#else
#define CC1200_DBG(...)
#endif

#define CC1200_POWER_ON()                                                                          \
	do {                                                                                       \
		P4DIR |= 0x10;                                                                     \
		P4SEL &= ~0x10;                                                                    \
		P4OUT &= ~0x10;                                                                    \
	} while (0);
#define CC1200_POWER_OFF()                                                                         \
	do {                                                                                       \
		P4DIR |= 0x10;                                                                     \
		P4SEL &= ~0x10;                                                                    \
		P4OUT |= 0x10;                                                                     \
	} while (0);
#define CC1200_RESET_PIN_LOW()                                                                     \
	do {                                                                                       \
		P8DIR |= 0x01;                                                                     \
		P8SEL &= ~0x01;                                                                    \
		P8OUT &= ~0x01;                                                                    \
	} while (0);
#define CC1200_RESET_PIN_HIGH()                                                                    \
	do {                                                                                       \
		P8DIR |= 0x01;                                                                     \
		P8SEL &= ~0x01;                                                                    \
		P8OUT |= 0x01;                                                                     \
	} while (0);

// NB-IoT 및 LoRa 보조중계기 동작에서는 아래 변수가 필요 없으나
// RF424/433 제품 코드와 맞추기 위해 유지. (참조 못하도록 static 정의.)
static BOOL drivebyFlag = FALSE;
static BOOL PendingCarrierDetect = FALSE;

struct {
	uint8 txOK;
	uint8 taskId;
	uint8 mode;
	uint8 ch;
	uint8 seqNo;
	uint8 ackNo;
	uint8 worDuty;
	uint8 maxRxLen;
	uint8 rxStatus;
	uint16 myPAN;
	uint32 myNWK;
} rfConfig;

struct {
	uint8 retry;
	uint8 msgType;
	uint8 seq;
	uint8 ackNum;
	uint8 preamble;
	uint8 len;
	uint8 txOk;
	uint8 tx_mode;
	uint16 preamble_count;
	uint16 retryTimeout;
	uint32 destAddr;
	uint8 data[LEN_RF_BUF];
} txPacket;

static int rxBuf_rptr = 0;
static int rxBuf_wptr = 0;

typedef struct {
	uint8 valid;
	uint8 len;
	int8 rssi;
	uint8 lqi; // link quality indicator
	uint8 data[LEN_RF_BUF];
} rxPacket_t;

rxPacket_t rxPacket[NUM_RF_RX_BUF];

uint8 rxTempBuff[LEN_RF_BUF];
uint8 rxTempLen;

/**
 * @brief Get the default channel (= config/control channel)
 *        본래 제어용 채널(설정 채널)은 보편적으로 CH0을 사용하지만,
 *        RF424의 경우 개발단계에서 CH6을 사용하였으며, 호환성을 이유로 유지.
 *
 * @return uint8 default channel
 */
uint8 CC1200_getDefaultChannel()
{
	// 424/447MHz 사용시에는 6번 채널을, 433MHz에서는 0번 채널을 default로 사용함
#if FREQ_BAND_424
	return 6;
#else
	return 0;
#endif
}

void CC1200_idle()
{
	trxSpiCmdStrobe(CC120X_SIDLE);
}
void CC1200_spwd()
{
	trxSpiCmdStrobe(CC120X_SPWD);
}
void CC1200_startTx()
{
	trxSpiCmdStrobe(CC120X_STX);
}
void CC1200_Rx()
{
	trxSpiCmdStrobe(CC120X_SRX);
}
void CC1200_wor()
{
	trxSpiCmdStrobe(CC120X_SWOR);
}
void CC1200_rxFlush()
{
	trxSpiCmdStrobe(CC120X_SFRX);
}
void CC1200_txFlush()
{
	trxSpiCmdStrobe(CC120X_SFTX);
}
int CC1200_getChannel()
{
	return rfConfig.ch;
}
int CC1200_getRxLen()
{
	return rfConfig.maxRxLen;
}

// CC1200 sw reset used in CC1200_open(). but, not now
// static void CC1200_swReset()    { trxSpiCmdStrobe(CC120X_SRES); }
static void CC1200_hwReset()
{
	DISABLE_CC1200_INTERRUPT();

	CC1200_RESET_PIN_HIGH();

	CC1200_RESET_PIN_LOW();
	MISC_delayMs(200); //200msec이상 필요

	CC1200_RESET_PIN_HIGH();
	MISC_delayMs(50);
}

void CC1200_clearRxBuf()
{
	halIntState_t intState;
	HAL_ENTER_CRITICAL_SECTION(intState);

	rxBuf_wptr = rxBuf_rptr = 0;
	memset(&rxPacket, 0, sizeof(rxPacket));

	HAL_EXIT_CRITICAL_SECTION(intState);
}

void CC1200_writeRegister(uint16 addr, uint8 value)
{
	uint8 writeByte = value;
	cc120xSpiWriteReg(addr, &writeByte, 1);
}

uint8 CC1200_readRegister(uint16 addr)
{
	uint8 value = 0;

	cc120xSpiReadReg(addr, &value, 1);
	return value;
}

void CC1200_changeChannel(uint8 ch)
{
	if (ch == rfConfig.ch) {
		return;
	}

	rfConfig.ch = ch;

	halIntState_t intState;

	HAL_ENTER_CRITICAL_SECTION(intState);

	CC1200_setChannel(ch);

	HAL_EXIT_CRITICAL_SECTION(intState);
	CC1200_DBG("New Channel = %d\n", ch);
}

void CC1200_setChannel(uint8 ch)
{
	rfConfig.ch = ch;
	uint32 value = 0;

	for (int i = 0; i < 3; i++) {
		value *= 0x100;
#if FREQ_BAND_433
		value += ChannelRf433[ch][i];
#else
		value += ChannelRF424[ch][i];
#endif
	}

	value += conf.freqOffset;

	uint8 freq[3];
	freq[2] = (uint8)(value % 0x100);
	freq[1] = (uint8)((value % 0x10000) / 0x100);
	freq[0] = (uint8)(value / 0x10000);

	// cc1200 register index와 freq의 index가 반대임에 유의
	cc120xSpiWriteReg(CC120X_FREQ2, &freq[0], 1);
	cc120xSpiWriteReg(CC120X_FREQ1, &freq[1], 1);
	cc120xSpiWriteReg(CC120X_FREQ0, &freq[2], 1);

	CC1200_cal();
}

void CC1200_setOutputPower(int level)
{
	uint8 pa_cfg1[3] = { 0x3f, 0x1f, 0x11 }; // 0 : 14dbm , 1 : 0dbm, 2 : -6dbm
	if (level < 0 || level > 2) {
		level = 0; // +14dBm
	}

	CC1200_writeRegister(CC120X_PA_CFG1, pa_cfg1[level]);
}

int8 CC1200_readRSSI()
{
	uint8 rssi0 = CC1200_readRegister(CC120X_RSSI0);
	uint8 rssi1 = CC1200_readRegister(CC120X_RSSI1);

	CC1200_DBG("RSSI #0:%02x RSSI #1:%02x\n", rssi0, rssi1);

	if ((rssi0 & 0x01) == 0 || rssi1 == 128) {
		rssi1 = 0;
	}

	return rssi1;
}

BOOL CC1200_readPreambleDetect()
{
	uint8 modem_status = CC1200_readRegister(CC120X_MODEM_STATUS1);

	if ((modem_status & 0x01) == 1) { //PQT_VALID
		return TRUE;
	} else {
		return FALSE;
	}
}

void CC1200_open(int ch)
{
	//CC1200_swReset();// TCXO 사용시 no use

	for (uint16 i = 0; i < (sizeof(PreferredSettings424) / sizeof(SpiRegisterSetting_t)); i++) {
		CC1200_writeRegister(PreferredSettings424[i].addr, PreferredSettings424[i].data);
	}

#if FREQ_BAND_433
	CC1200_writeRegister(CC120X_DEVIATION_M, 0x39);
	CC1200_writeRegister(CC120X_MODCFG_DEV_E, 0x09);
	CC1200_writeRegister(CC120X_CHAN_BW, 0x65);
	CC1200_writeRegister(CC120X_AGC_REF, 0x32);
#endif

	CC1200_setChannel(ch);
	CC1200_idle();
}

void CC1200_calCRC(uint8 *data, int len, uint16 *crc)
{
	uint16 ccrc = 0;
	for (int i = 0; i < len; i++) {
		ccrc = CRC16_ccittByte(ccrc, *data++);
	}

	*crc = ccrc;
}

void CC1200_cal()
{
	trxSpiCmdStrobe(CC120X_SCAL);

	int result = 0;
	int count;

	for (count = 0; count < 1000; count++) {
		if (CC1200_readRegister(CC120X_MARCSTATE) == 0x41) {
			result = 1;
			break;
		}
		MISC_delayUs(1);
	}

	if (result) {
		CC1200_DBG("Calibrate radio (%d) \n", count);
	} else {
		CC1200_DBG("Calibrate radio (Fail)\n");
	}
}

static void calibrateRCOsc(void)
{
	// Read current register value
	uint8 temp = CC1200_readRegister(CC120X_WOR_CFG0);

	// Mask register bit fields and write new values
	temp = (temp & 0xF9) | (0x02 << 1);

	// Write new register value
	CC1200_writeRegister(CC120X_WOR_CFG0, temp);

	CC1200_idle();

	// Disable RC calibration
	temp = (temp & 0xF9) | (0x00 << 1);
	CC1200_writeRegister(CC120X_WOR_CFG0, temp);
}

void CC1200_worDuty(int duty)
{
	if (duty == WOR_LONG_DUTY) {
		// medium high 0x17, 0xff -> 6ms on, 5.05s off
		CC1200_writeRegister(CC120X_WOR_EVENT0_MSB,
				     0x17); // 0x0E - 3.14s // 0x17 - 5.05s // 0x10 3.58s
		CC1200_writeRegister(CC120X_WOR_EVENT0_LSB, 0xff);
	} else { // 2ms on, 17ms off --> wakeup ratio = 2/19 = 10.5%
		// medium high 0x00, 0x1C -> 6ms on, 17ms off
		CC1200_writeRegister(CC120X_WOR_EVENT0_MSB, 0x00);
		CC1200_writeRegister(CC120X_WOR_EVENT0_LSB, 0x1C);
	}
	rfConfig.worDuty = duty;
}

void CC1200_syncWordEnable()
{
	CC1200_writeRegister(CC120X_SYNC3, 0xA6);
	CC1200_writeRegister(CC120X_SYNC2, 0x5A);
	CC1200_writeRegister(CC120X_SYNC1, 0x96);
	CC1200_writeRegister(CC120X_SYNC0, 0x56);
}

void CC1200_syncWord_disable()
{
	CC1200_writeRegister(CC120X_SYNC3, 0x00);
	CC1200_writeRegister(CC120X_SYNC2, 0x00);
	CC1200_writeRegister(CC120X_SYNC1, 0x00);
	CC1200_writeRegister(CC120X_SYNC0, 0x00);
}

void CC1200_preambleEnable()
{
	CC1200_writeRegister(CC120X_PREAMBLE_CFG1, 0x37);
}

void CC1200_preamble_disable()
{
	CC1200_writeRegister(CC120X_PREAMBLE_CFG1, 0x00);
}

void CC1200_setTxMode()
{
	DISABLE_CC1200_INTERRUPT()

	CC1200_syncWord_disable();
	CC1200_preamble_disable();

	rfConfig.mode = RF_MODE_TX;

	SET_CC1200_INTERRUPT(FALLING_EDGE);
}

void CC1200_setRxMode()
{
	rfConfig.mode = RF_MODE_RX;

	DISABLE_CC1200_INTERRUPT()

	CC1200_clearRxBuf();

	CC1200_writeRegister(CC120X_PKT_LEN, rfConfig.maxRxLen);

	CC1200_writeRegister(CC120X_PKT_CFG0, 0x00);
	CC1200_writeRegister(CC120X_FIFO_CFG, 0x00);
	CC1200_writeRegister(CC120X_IOCFG0, 0x06);

	CC1200_syncWordEnable();
	CC1200_preambleEnable();

	rxTempLen = 0;

	// Calibrate the RCOSC
	calibrateRCOsc();

	SET_CC1200_INTERRUPT(RISING_EDGE);
}

void CC1200_rfOff()
{
	// brief : Instead of 'CC1200_setSleep()'
	// DISABLE_CC1200_INTERRUPT();
	// CC1200_spwd();

	// 본래 CSN, MISO, CC1200 GPIO0(=Interrupt signal pin) 을 output으로 해도
	// 정상적으로 동작해야 하지만, CC1200 power off 후 일부 pin의 output high로 인해
	// CC1200이 동작하고 그에 따라 해당 Pin에서 output low했을 때 short가 발생함.

	CC1200_RESET_PIN_LOW();

	// set input - CSN(P3.0), MISO(P3.2)
	P3SEL &= ~0x05;
	P3DIR &= ~0x05;

	// set output low - MOSI(P3.1), SCLK(P3.4)
	P3SEL &= ~0x0A;
	P3DIR |= 0x0A;
	P3OUT &= ~0x0A;

	// set output low - CC1200 GPIO2(P2.2), CC1200 GPIO3(P2.4), CC1200 GPIO0(P2.7)
	P2SEL &= ~0x94;
	P2DIR |= 0x94;
	P2OUT &= ~0x94;

	CC1200_POWER_OFF();
}

void CC1200_rfOn()
{
	trxRfSpiInterfaceInit(2);
	P2DIR &= ~CC1200_GPIO0; // make input
	CC1200_POWER_ON();

	// H/W reset 없이 RESET pin을 HIGN로 해도 되지만,
	// Timing 문제가 발생할 수 있으므로 H/W reset으로 동작 시킴.
	CC1200_hwReset();
}

void CC1200_rx_short()
{
	CC1200_setRxMode();
	CC1200_worDuty(WOR_SHORT_DUTY);
	CC1200_wor();
}

void CC1200_sendMessage2RF(uint8 *msg, int len, int preamble)
{
	if (len > FIFO_SIZE) {
		len = FIFO_SIZE;
	}

	// Sync Word
	txPacket.data[0] = SYNC_BYTE;
	txPacket.data[1] = NSYNC_BYTE;

	// RF Header
	RfMessageHeader_t *pHead = (RfMessageHeader_t *)&txPacket.data[2];

	pHead->pan[0] = rfConfig.myPAN & 0x00ff;
	pHead->pan[1] = (rfConfig.myPAN & 0xff00) >> 8;

	for (int i = 0; i < 4; i++) {
		pHead->nwk[i] = 0;
		pHead->srcAddr[i] = BREAK_UINT32(rfConfig.myNWK, i);
		pHead->destAddr[i] = 0;
	}

	pHead->flag = 0;
	pHead->ack = 0;
	pHead->seq = 0;

	// Data
	uint8 *pData = &txPacket.data[sizeof(RfMessageHeader_t) + 2]; // 2 : Sync Word
	memcpy(pData, msg, len);

	pHead->nLen = len;
	uint16 mLen = len + 12; // 12:nwk header
	memcpy(pHead->mLen, &mLen, 2);

	int txlen = mLen + 8; // 8: mac header

	uint16 crc = 0;
	CC1200_calCRC(&txPacket.data[2], txlen, &crc);
	txPacket.data[txlen + 2] = crc & 0x00ff;
	txPacket.data[txlen + 3] = (crc & 0xff00) >> 8;

	txlen += 4; // 2 : Sync Word , 2 : crc
	txPacket.len = txlen;
	txPacket.preamble = preamble;
	txPacket.retry = 1;

	OSAL_startEventTimer(rfConfig.taskId, RADIO_EVENT_RF_TX, (uint32)10);
}

void Tx_Data(void)
{
	uint8 txbuf[LEN_RF_BUF];
	uint8 txlen = txPacket.preamble_count + txPacket.len;
	if (txlen > AVAILABLE_BYTES_IN_TX_FIFO) {
		uint8 data_len = AVAILABLE_BYTES_IN_TX_FIFO - txPacket.preamble_count;
		memset(txbuf, 0xAA, txPacket.preamble_count);
		memcpy(&txbuf[txPacket.preamble_count], txPacket.data, data_len);
		cc120xSpiWriteTxFifo(txbuf, AVAILABLE_BYTES_IN_TX_FIFO);
		txPacket.preamble_count = 0;
		txPacket.len -= data_len;
		memcpy(txPacket.data, &txPacket.data[data_len], txPacket.len);
	} else {
		txPacket.tx_mode = 0; // Fixed

		CC1200_writeRegister(CC120X_PKT_CFG0, 0x00);
		CC1200_writeRegister(CC120X_FIFO_CFG, 0x00);
		CC1200_writeRegister(CC120X_IOCFG0, 0x06);

		uint8 data_len = txlen - txPacket.preamble_count;
		memset(txbuf, 0xAA, txPacket.preamble_count);
		memcpy(&txbuf[txPacket.preamble_count], txPacket.data, data_len);

		// Write packet to TX FIFO
		cc120xSpiWriteTxFifo(txbuf, txlen);
	}
}

static void radioTx(uint8 *msg, int len, int preamble)
{
	rfConfig.txOK = 0;
	rfConfig.ackNo = 0;

	if (drivebyFlag == FALSE) { // DRIVEBY Carrier ignore
		for (int i = 0; i < 8; i++) {
			uint8 tx_rssi = P2IN & 0x20;
			if (tx_rssi) {
				uint16 delay = (MISC_getRandomNum() & 0xFF) + 1;
				// CC1200_DBG("delay[%02d]\n",delay);
				MISC_delayMs(delay);
			} else {
				break;
			}
		}
	}

	CC1200_setTxMode();
	CC1200_startTx();

	uint8 txbuf[LEN_RF_BUF];
	uint8 txlen;
	if (preamble == LONG_PREAMBLE) {
		txPacket.tx_mode = 1; // Inifinite
		txPacket.preamble_count = 815;

		CC1200_writeRegister(CC120X_PKT_CFG0, INFINITE_PACKET_LENGTH_MODE);
		CC1200_writeRegister(CC120X_FIFO_CFG, 0x78);
		CC1200_writeRegister(CC120X_IOCFG0, 0x02);

		txlen = (txPacket.preamble_count + len) % 256;
		CC1200_writeRegister(CC120X_PKT_LEN, txlen);

		memset(txbuf, 0xAA, FIFO_SIZE);
		// Write packet to TX FIFO
		cc120xSpiWriteTxFifo(txbuf, FIFO_SIZE);

		int wait_msec = (txPacket.preamble_count + len) * 9;
		txPacket.preamble_count -= 128;
		// Wait for packet to be sent
		CC1200_DBG("Tx wait (long preamble, %d)\n", txPacket.preamble_count);
		for (int i = 0; i < wait_msec; i++) {
			MISC_delayMs(1);
			if (rfConfig.txOK) {
				break;
			}
		}
	} else {
		txPacket.preamble_count = 20;
		txlen = len + txPacket.preamble_count;
		if (txlen > FIFO_SIZE) {
			txPacket.tx_mode = 1; // Inifinite

			CC1200_writeRegister(CC120X_PKT_CFG0, INFINITE_PACKET_LENGTH_MODE);
			CC1200_writeRegister(CC120X_FIFO_CFG, 0x78);
			CC1200_writeRegister(CC120X_IOCFG0, 0x02);

			txlen = (txPacket.preamble_count + len) % 256;
			CC1200_writeRegister(CC120X_PKT_LEN, txlen);

			Tx_Data();
		} else {
			txPacket.tx_mode = 0; // Fixed

			CC1200_writeRegister(CC120X_PKT_CFG0, FIXED_PACKET_LENGTH_MODE);
			CC1200_writeRegister(CC120X_FIFO_CFG, 0x00);
			CC1200_writeRegister(CC120X_IOCFG0, 0x06);

			memset(txbuf, 0xAA,
			       txPacket.preamble_count); // Preamble
			memcpy(&txbuf[txPacket.preamble_count], msg, len);

			CC1200_writeRegister(CC120X_PKT_LEN, txlen);

			// Write packet to TX FIFO
			cc120xSpiWriteTxFifo(txbuf, txlen);
		}
		// Wait for packet to be sent
		CC1200_DBG("Tx wait (short preamble, %d)\n", txPacket.preamble_count);
		for (int i = 0; i < txlen * 9; i++) {
			MISC_delayMs(1);
			if (rfConfig.txOK) {
				break;
			}
		}
	}

	MISC_delayMs(50);
}

static BOOL MyMessage(uint8 *data)
{
	RfMessageHeader_t *p = (RfMessageHeader_t *)data;

	uint16 rPAN = p->pan[0] | (p->pan[1] << 8);
	uint32 rNWK = 0;
	for (int i = 3; i >= 0; i--) {
		rNWK <<= 8;
		rNWK |= p->nwk[i];
	}

	BOOL result = FALSE;

	if (rNWK == BROADCAST_ADDR) {
		result = TRUE;
	} else {
		if (rPAN == rfConfig.myPAN &&
		    (rNWK == rfConfig.myNWK || rNWK == DEFAULT_GROUP_SCAN_ADDR)) {
			result = TRUE;
		}
	}

	if (result == FALSE) {
		CC1200_DBG("Not my message: pan[rx:%04x mine:%04x] nwk[rx:%08lx mine:%08lx]\n",
			   rPAN, rfConfig.myPAN, rNWK, rfConfig.myNWK);
	}

	return result;
}

static int checkCRC(uint8 *data)
{
	RfMessageHeader_t *p = (RfMessageHeader_t *)data;

	uint16 len = 0;
	memcpy(&len, p->mLen, 2);

#if 0
    CC1200_DBG("pan : %02x%02x\n", p->pan[1], p->pan[0]);
    CC1200_DBG("nwk : %d.%d.%d.%d\n", p->nwk[3], p->nwk[2], p->nwk[1], p->nwk[0]);
    CC1200_DBG("mlen: %d(%04x)\n", len, len);
    CC1200_DBG("src : %d.%d.%d.%d\n", p->srcAddr[3],  p->srcAddr[2],  p->srcAddr[1],  p->srcAddr[0]);
    CC1200_DBG("dest: %d.%d.%d.%d\n", p->destAddr[3], p->destAddr[2], p->destAddr[1], p->destAddr[0]);
    CC1200_DBG("flag(%d) seq(%d) ack(%d) nLen(%d)\n", p->flag, p->seq, p->ack, p->nLen);
#endif

	len += 8; // nwk(4) + pan(2) + mlen(2)

	if (len > LEN_RF_BUF) {
		CC1200_DBG("CRC Error (data len=%d, max len=%d)\n", len, LEN_RF_BUF);
		return FALSE;
	}

	uint16 ccrc = 0;
	CC1200_calCRC(data, len, &ccrc);

	uint16 rcrc = (data[len] & 0xFF) | (data[len + 1] << 8);

	if (ccrc != rcrc) {
		CC1200_DBG("CRC Error (calc CRC=%04x read CRC=%04x)\n", ccrc, rcrc);
	}

	return (ccrc == rcrc);
}

BOOL CC1200_testGetReceivedData(uint32 *pTime, uint8 *data, uint8 *pLen, uint8 *pRssi, BOOL *pCRC)
{
	halIntState_t intState;

	HAL_ENTER_CRITICAL_SECTION(intState);
	if (rxBuf_wptr == rxBuf_rptr) {
		HAL_EXIT_CRITICAL_SECTION(intState);
		return FALSE;
	}

	HAL_EXIT_CRITICAL_SECTION(intState);

	rxPacket_t *p = &rxPacket[rxBuf_rptr];

	*pCRC = checkCRC(p->data);
	*pTime = TIMER_getMsec();
	*pRssi = CC1200_getRxPower(p->rssi);
	*pLen = p->len;
	memcpy(data, p->data, p->len);

	HAL_ENTER_CRITICAL_SECTION(intState);
	if (++rxBuf_rptr >= NUM_RF_RX_BUF) {
		rxBuf_rptr = 0;
	}
	HAL_EXIT_CRITICAL_SECTION(intState);

	return TRUE;
}

uint32 CC1200_getParentAddr(uint32 addr)
{
	return 0;

#if 0 // 보조중계기 동작이 아닌 경우
    return (addr & MaskAddr[conf.depth-1]);
#endif
}

uint32 makeNextAddr(uint32 dest)
{
	return dest;

#if 0 // 보조중계기 동작이 아닌 경우
    uint32 next;

    if(dest == BROADCAST_ADDR || dest == DEFAULT_PDA_ADDR || dest == DEFAULT_GROUP_SCAN_ADDR
         || dest == rfConfig.myNWK || rfConfig.myNWK == DEFAULT_PDA_ADDR) {
        next = dest;
    } else {
        if(dest > rfConfig.myNWK) {
            next = dest & MaskAddr[conf.depth+1];
        } else if(dest < rfConfig.myNWK) {
            next = conf.nwk_addr & MaskAddr[conf.depth-1];
        }
    }

    return next;
#endif
}

BOOL CC1200_dataRequest(uint32 destAddr, uint8 ackFlag, uint8 retryCnt, byte *msg, uint8 len)
{
	halIntState_t intState;
	HAL_ENTER_CRITICAL_SECTION(intState);

	// Sync Word
	txPacket.data[0] = SYNC_BYTE;
	txPacket.data[1] = NSYNC_BYTE;

	// RF Header
	RfMessageHeader_t *pHead = (RfMessageHeader_t *)&txPacket.data[2];

	pHead->pan[0] = rfConfig.myPAN & 0x00ff;
	pHead->pan[1] = (rfConfig.myPAN & 0xff00) >> 8;

	uint32 nextAddr = makeNextAddr(destAddr);

	for (int i = 0; i < 4; i++) {
		pHead->nwk[i] = BREAK_UINT32(nextAddr, i);
		pHead->srcAddr[i] = BREAK_UINT32(rfConfig.myNWK, i);
		pHead->destAddr[i] = BREAK_UINT32(destAddr, i);
	}

	pHead->ack = rfConfig.ackNo;
	pHead->seq = 1;

#if 0 // 보조중계기 동작이 아닌 경우
    pHead->seq = rfConfig.seqNo++;
#endif

	if (pHead->ack) { // Delayed Ack
		ackFlag |= NWK_HDR_FLAG_ACK;
	}
	pHead->flag = ackFlag;

	// Data
	uint8 *pData = &txPacket.data[sizeof(RfMessageHeader_t) + 2]; // 2 : Sync Word
	memcpy(pData, msg, len);

	pHead->nLen = len;
	uint16 mLen = len + 12; // 12:nwk header
	memcpy(pHead->mLen, &mLen, 2);

	int txlen = mLen + 8; // 8: mac header

	uint16 crc = 0;
	CC1200_calCRC(&txPacket.data[2], txlen, &crc);
	txPacket.data[txlen + 2] = crc & 0x00ff;
	txPacket.data[txlen + 3] = (crc & 0xff00) >> 8;

	txlen += 4; // 2 : Sync Word , 2 : crc
	txPacket.len = txlen;

	uint8 msgType = *msg;
	txPacket.msgType = msgType;
	txPacket.destAddr = destAddr;
	txPacket.seq = pHead->seq;

	// preamble check
	if (SLAVE_longPreambleRequired(msgType)) {
		txPacket.preamble = LONG_PREAMBLE;
	} else {
		txPacket.preamble = SHORT_PREAMBLE;
	}

	// set retry cnt
	txPacket.retry = retryCnt;

	// set timeout value
	if (txPacket.preamble == LONG_PREAMBLE) {
		txPacket.retryTimeout = CC1200_LONG_TIMEOUT;
	} else {
		if (txPacket.retry == 1) {
			txPacket.retryTimeout = CC1200_RETX_TIMEOUT;
		} else {
			txPacket.retryTimeout = CC1200_RETX_TIMEOUT * 2;
		}
	}

#if 0 // 기존 AMI의 retry 및 timeout 설정 동작.
    if(SLAVE_reTxNeeded(msgType) && ackFlag == NWK_HDR_FLAG_DATA_REQ) {
        txPacket.retry = 3;
    } else if(msgType == MSG_PDA_NODE_CONF_SUCCESS){
        txPacket.retry = 2;
    } else {//NWK_HDR_FLAG_DATA_RESP
        txPacket.retry = 1;
    }

    uint8 timeout_arg;
    uint8 dest_depth = getDepth(destAddr);
    if(destAddr == DEFAULT_PDA_ADDR || destAddr == BROADCAST_ADDR){
        timeout_arg = 1;
    } else {
        if(conf.depth == dest_depth){
            timeout_arg = 1;
        }else{
            timeout_arg = ABS(conf.depth-dest_depth);
        }
    }

    if(drivebyFlag == TRUE){
        txPacket.retry = 1;
        txPacket.retryTimeout = 100;
    }else if(txPacket.preamble == LONG_PREAMBLE ||
             msgType == MSG_PDA_MASTER_SLAVE_METER_REQ){// Slave MT_Down 응답시간 고려
        txPacket.retryTimeout = CC1200_LONG_TIMEOUT;
    }else if(txPacket.retry == 1){
        txPacket.retryTimeout = CC1200_RETX_TIMEOUT;
    }else{
        txPacket.retryTimeout = CC1200_RETX_TIMEOUT*timeout_arg*2;
    }
#endif

	txPacket.txOk = FAIL;
	OSAL_startEventTimer(rfConfig.taskId, RADIO_EVENT_RF_TX, (uint32)10);

	HAL_EXIT_CRITICAL_SECTION(intState);

	MISC_delayMs(100);
	// printTxMsg(destAddr, ackFlag, msg, len);

	return TRUE;
}

void Send_Ack(uint32 destAddr, uint8 seqNum)
{
	halIntState_t intState;
	HAL_ENTER_CRITICAL_SECTION(intState);

	// Sync Word
	txPacket.data[0] = SYNC_BYTE;
	txPacket.data[1] = NSYNC_BYTE;

	// RF Header
	RfMessageHeader_t *pHead = (RfMessageHeader_t *)&txPacket.data[2];

	pHead->pan[0] = rfConfig.myPAN & 0x00ff;
	pHead->pan[1] = (rfConfig.myPAN & 0xff00) >> 8;

	uint32 nextAddr = makeNextAddr(destAddr);

	for (int i = 0; i < 4; i++) {
		pHead->nwk[i] = BREAK_UINT32(nextAddr, i);
		pHead->srcAddr[i] = BREAK_UINT32(rfConfig.myNWK, i);
		pHead->destAddr[i] = BREAK_UINT32(destAddr, i);
	}

	pHead->flag = NWK_HDR_FLAG_ACK;
	pHead->ack = seqNum;
	pHead->seq = rfConfig.seqNo;

	pHead->nLen = 0;
	uint16 mLen = 12; // 12:nwk header
	memcpy(pHead->mLen, &mLen, 2);

	int txlen = mLen + 8; // 8: mac header

	uint16 crc = 0;
	CC1200_calCRC(&txPacket.data[2], txlen, &crc);
	txPacket.data[txlen + 2] = crc & 0x00ff;
	txPacket.data[txlen + 3] = (crc & 0xff00) >> 8;

	txlen += 4; // 2 : Sync Word , 2 : crc
	txPacket.len = txlen;
	txPacket.preamble = SHORT_PREAMBLE;
	txPacket.retry = 1;
	txPacket.retryTimeout = 100;
	txPacket.txOk = SUCCESS;

	OSAL_startEventTimer(rfConfig.taskId, RADIO_EVENT_RF_TX, (uint32)10);

	HAL_EXIT_CRITICAL_SECTION(intState);
}

uint8 CC1200_getRxPower(int8 rs)
{
	uint8 rxPower;
	rxPower = ABS(rs + CC1200_RSSI_OFFSET);

	if (rxPower < RX_POWER_LIMITED_VAL) {
		rxPower = RX_POWER_LIMITED_VAL;
	}

	return rxPower;
}

void messageRxProcess(rxPacket_t *pPacket)
{
	uint8 *pMsg = pPacket->data;

	RfMessageHeader_t *pHead = (RfMessageHeader_t *)pMsg;
	uint8 *pBody = &pPacket->data[sizeof(RfMessageHeader_t)];
	uint8 msgType = *(pBody + 0);
	uint8 reqType = *(pBody + 5);

	if (pPacket->len == 0 || checkCRC(pMsg) == 0 || MyMessage(pMsg) == FALSE) {
		SLAVE_activeStart(1);
#if 0 // 보조중계기 동작이 아닌 경우
        if(Car_Meter_Confirm(msgType, reqType) || drivebyFlag == TRUE){// Car Meter // Group Meter
            SLAVE_activeStart(10);
        }else{
            SLAVE_activeStart(1);
        }
#endif
		return;
	}

	uint32 srcAddr = BUILD_UINT32(pHead->srcAddr[0], pHead->srcAddr[1], pHead->srcAddr[2],
				      pHead->srcAddr[3]);
	uint32 destAddr = BUILD_UINT32(pHead->destAddr[0], pHead->destAddr[1], pHead->destAddr[2],
				       pHead->destAddr[3]);

	// Rx ACK process
	if (pHead->flag & NWK_HDR_FLAG_ACK) {
		if (txPacket.destAddr == srcAddr && txPacket.seq == pHead->ack) {
			txPacket.retry = 0; // stop retransmit
			txPacket.txOk = SUCCESS;
			SLAVE_DataRequestCfm(srcAddr, txPacket.msgType, pHead->seq, (BOOL)SUCCESS);
		}
	}

	// Tx ACK & report to APP
	if (pHead->flag & NWK_HDR_FLAG_DATA_REQ || pHead->flag & NWK_HDR_FLAG_DATA_RESP) {
		if (destAddr != BROADCAST_ADDR) {
			if (SLAVE_sendAckRequired(msgType, reqType)) {
				rfConfig.ackNo = pHead->seq;
				Send_Ack(srcAddr, pHead->seq);
				//MISC_delayMs(100);
			}
		}

		SLAVE_runMessage(srcAddr, pBody, pHead->nLen, CC1200_getRxPower(pPacket->rssi));
	}
}

event32_t CC1200_tasks(uint8 taskId, event32_t events)
{
	event32_t rEvents = 0;
	if (events & RADIO_EVENT_RF_TX) {
		CC1200_DBG("Task (RF Tx)\n");
		if (txPacket.retry) {
			txPacket.retry--;

			SLAVE_activeStart(10); //Timeout 연장
			radioTx(txPacket.data, txPacket.len, txPacket.preamble);
			SLAVE_activeStart(10);

			OSAL_startEventTimer(rfConfig.taskId, RADIO_EVENT_RF_TX,
					     (uint32)txPacket.retryTimeout);

			CC1200_rx_short(); //yikim timer 설정 이후 실행
		} else {
			// TODO : Have to check this procedure
			if (txPacket.txOk == FAIL) {
				SLAVE_DataRequestCfm(txPacket.destAddr, txPacket.msgType,
						     rfConfig.seqNo, (BOOL)FAIL);
			}

			BOOL preamble = CC1200_readPreambleDetect();

			if (drivebyFlag == TRUE || PendingCarrierDetect == TRUE) {
				if (drivebyFlag == TRUE) {
					drivebyFlag = FALSE;
				}
				SLAVE_activeStart(10);
			} else {
				if (preamble == TRUE) {
					SLAVE_activeStart(10);
				} else {
					SLAVE_activeStart(1);
				}
			}

			CC1200_DBG("Tx preamble = %d \n", preamble);
		}
		return (events ^ RADIO_EVENT_RF_TX);
	}

	if (events & RADIO_EVENT_RF_RX) {
		CC1200_DBG("Task (RF Rx)\n");
		int msgExist = 0;
		halIntState_t intState;

		HAL_ENTER_CRITICAL_SECTION(intState);
		if (rxBuf_wptr != rxBuf_rptr) {
			msgExist = 1;
		}
		HAL_EXIT_CRITICAL_SECTION(intState);

		if (msgExist) {
			rxPacket_t *p = &rxPacket[rxBuf_rptr];
			messageRxProcess(p);

			// RF Rx data는 ISR에서 Data를 읽고 rxPacket에 저장(rxBuf_wptr)
			HAL_ENTER_CRITICAL_SECTION(intState);
			if (++rxBuf_rptr >= NUM_RF_RX_BUF) {
				rxBuf_rptr = 0;
			}
			HAL_EXIT_CRITICAL_SECTION(intState);

			// 신규 데이터 발생 시 ISR에서 event를 등록.
			// OSAL_startEventTimer(rfConfig.taskId, RADIO_EVENT_RF_RX, (uint32)50);
		}
		return (events ^ RADIO_EVENT_RF_RX);
	}

	if (events & RADIO_MSG_CHECK) {
		CC1200_DBG("Task (Message check)\n");
		if (rfConfig.rxStatus == 0) {
			halIntState_t intState;
			HAL_ENTER_CRITICAL_SECTION(intState);
			uint8 rxBytes = CC1200_readRegister(CC120X_NUM_RXBYTES);
			cc120xSpiReadRxFifo(rxTempBuff, rxBytes);
			rxTempLen = rxBytes;
			if (rxBytes >= 7) {
				uint8 rxLen = rxTempBuff[6] + 10; // 10 : pan 2, nwk 4, len 2, crc 2
				if (rxLen >= MIN_LEN_RF_BUF && rxLen < LEN_RF_BUF &&
				    rxBytes <= rxLen) {
					CC1200_writeRegister(CC120X_PKT_LEN, rxLen);
				}
			}
			HAL_EXIT_CRITICAL_SECTION(intState);
		}
		return (events ^ RADIO_MSG_CHECK);
	}

	return rEvents;
}

void CC1200_init(uint8 taskId)
{
	memset(&rfConfig, 0, sizeof(rfConfig));

	rfConfig.taskId = taskId;
	rfConfig.txOK = 0;
	rfConfig.ackNo = 0;
	rfConfig.mode = RF_MODE_RX;
	rfConfig.maxRxLen = 0x80;

	rfConfig.myPAN = conf.pan_id;
	rfConfig.myNWK = conf.nwk_addr;

	memset(&txPacket, 0, sizeof(txPacket));
	memset(&rxPacket, 0, sizeof(rxPacket));

	rxBuf_rptr = rxBuf_wptr = 0;
}

int get_myChannel()
{
	int ch = 6;

	for (int pos = 0; pos < 4; pos++) {
		uint8 lastNonZeroField = BREAK_UINT32(conf.nwk_addr, pos);
		if (lastNonZeroField != 0) {
			ch = (lastNonZeroField % 16) + 1;
			break;
		}
	}
	return ch;
}

void CC1200_setReady()
{
	CC1200_open(get_myChannel());
	CC1200_setRxMode();
	CC1200_worDuty(WOR_SHORT_DUTY);

	SET_CC1200_INTERRUPT(RISING_EDGE);

	CC1200_wor();
}

void CC1200_setSleep()
{
	DISABLE_CC1200_INTERRUPT();
	CC1200_spwd();
}

void CC1200_reset()
{
	CC1200_DBG("reset\n");

	CC1200_hwReset();

	DISABLE_CC1200_INTERRUPT();

	CC1200_idle();

	CC1200_open(rfConfig.ch);
	CC1200_setRxMode();

	CC1200_rxFlush();
	CC1200_txFlush();

	SET_CC1200_INTERRUPT(RISING_EDGE);

#if defined(AUX_REPEATER)
	SLAVE_activeStart(0);
#else
	if (REEDSensorReq == TRUE) {
		CC1200_worDuty(WOR_SHORT_DUTY);
		CC1200_wor();
	} else {
		SLAVE_activeStart(0);
	}
#endif
}

/***************** radio interrupt ***************/

BOOL CC1200_isRadioTxDone()
{
	return (rfConfig.txOK == 1) ? TRUE : FALSE;
}

void CC1200_radioTx()
{
	rfConfig.txOK = 1;
}

void CC1200_read_rxByte()
{
	rxTempLen = 0;
	rfConfig.rxStatus = 0;
	OSAL_startEventTimer(rfConfig.taskId, RADIO_MSG_CHECK, (uint32)100);
}

void CC1200_radioRx()
{
	rfConfig.rxStatus = 1;
	uint8 rxBytes = CC1200_readRegister(CC120X_NUM_RXBYTES);
	if (rxBytes == 0 || rxBytes > LEN_RF_BUF) {
		CC1200_rxFlush();
	} else {
		cc120xSpiReadRxFifo(&rxTempBuff[rxTempLen], rxBytes);

		if (((rxBuf_wptr + 1) % NUM_RF_RX_BUF) == rxBuf_rptr) {
			// Rx data not exist
		} else {
			uint8 len = rxBytes + rxTempLen;
			rxPacket_t *p = &rxPacket[rxBuf_wptr];
			memcpy(p->data, rxTempBuff, len);
			p->len = len;
			p->valid = rxTempBuff[len - 1] & 0x80;
			p->rssi = rxTempBuff[len - 2];
			p->lqi = rxTempBuff[len - 1] & 0x7f;

			if (++rxBuf_wptr >= NUM_RF_RX_BUF) {
				rxBuf_wptr = 0;
			}

			OSAL_startEventTimer(rfConfig.taskId, RADIO_EVENT_RF_RX, (uint32)10);
		}
		CC1200_rxFlush();
	}
	CC1200_writeRegister(CC120X_PKT_LEN, 0x80);
	CC1200_wor();
}

uint8 CC1200_getRfMode()
{
	return rfConfig.mode;
}

void CC1200_isr()
{
	if (rfConfig.mode == RF_MODE_TX) {
		if (txPacket.tx_mode == 1) { // Inifinite
			if (txPacket.preamble_count >= AVAILABLE_BYTES_IN_TX_FIFO) {
				CC1200_DBG("ISR (Tx preamble=%d)\n", txPacket.preamble_count);
				uint8 txbuf[LEN_RF_BUF];
				uint8 txlen = AVAILABLE_BYTES_IN_TX_FIFO;
				memset(txbuf, 0xAA, txlen);
				// Write packet to TX FIFO
				cc120xSpiWriteTxFifo(txbuf, txlen);
				txPacket.preamble_count -= AVAILABLE_BYTES_IN_TX_FIFO;
			} else {
				CC1200_DBG("ISR (Tx data)\n");
				Tx_Data();
			}
		} else {
			CC1200_DBG("ISR (Tx finish)\n");
			PORT_SENSOR_IES &= ~BM(PORT_CC1200_GPIO0); // P2.7 Low/Hi edge
			CC1200_radioTx();
			CC1200_txFlush();
		}
	} else {
		CC1200_DBG("ISR (Rx)\n");
		CC1200_radioRx();
	}
}

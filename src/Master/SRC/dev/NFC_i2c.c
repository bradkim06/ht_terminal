///////MSP430I2C.c

#include <msp430.h>
#include "common_header.h"
#include "NFC_i2c.h"
#include "rtcAlarm.h"
#include "check_meter_misc.h"
#include "port_desc.h"

#include "nfcProtocol.h"
#include "uart.h"
#include "battery.h"

#define NFC_I2C_DEVICE_ADDR 0x55

#define I2C_DEVICE_ADDR NFC_I2C_DEVICE_ADDR

#define MAX_I2C_RX_BUFF_SIZE 16 + 2
#define MAX_I2C_TX_BUFF_SIZE 16 + 2

uint8 *I2cRxData; // Pointer to RX data
uint8 I2cRxPos;
uint8 I2cRxByteLen;

uint8 *I2cTxData; // Pointer to TX data
uint8 I2cTxByteCtr;

BOOL IsI2cComplete;

uint8 RxBuffer[MAX_I2C_RX_BUFF_SIZE]; // Allocate 16 byte of RAM
uint8 TxBuffer[MAX_I2C_TX_BUFF_SIZE]; // Allocate 16 byte of RAM

/* yikim 추후 예정
static const uint8 Default_NDEF_Message[] = {
        0x03, 0x12, 0xD1, 0x01, 	//HITEC NTAG
        0x0E, 0x54, 0x02, 0x65,
        0x6E, 0x48, 0x49, 0x54,
        0x45, 0x43, 0x20, 0x4E,
        0x54, 0x41, 0x47, 0x20,
        0xFE, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00};
*/

static const uint8 DefaultBeginingOfMemory[] = {
	0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE1, 0x10,
	0x6D, 0x00, 0x03, 0x12, 0xD1, 0x01, //HITEC NTAG
	0x0E, 0x54, 0x02, 0x65, 0x6E, 0x48, 0x49, 0x54, 0x45, 0x43, 0x20, 0x4E, 0x54, 0x41,
	0x47, 0x20, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8 NullBlock[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static const uint8 DefaultPage56[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF };

static const uint8 DefaultPage57[] = { 0x00, 0x00, 0x00, 0x00, 0x3A, 0x5A, 0xA5, 0xA3, //password
				       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static const uint8 DefaultPage58[] = { 0x01, 0x00, 0xF8, 0x48, 0x08, 0x01, 0x00, 0x00,
				       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

//static const uint8 Default_NDEF_Message_length = sizeof(Default_NDEF_Message);
static const uint8 DefaultBeginingOfMemoryLength = sizeof(DefaultBeginingOfMemory);

BOOL I2C_rxMultipleBytes(uint8 slaveAddr, uint8 *data, uint8 len)
{
	uint8 status = NTAG_RET_OK;

	memset(RxBuffer, 0, MAX_I2C_RX_BUFF_SIZE);

	I2cRxData = (uint8 *)RxBuffer; // Start of RX buffer

	if (len > MAX_I2C_RX_BUFF_SIZE) {
		len = MAX_I2C_RX_BUFF_SIZE;
	}
	I2cRxByteLen = len; // Load RX byte counter
	I2cRxPos = 0;
	IsI2cComplete = FALSE;

	P10SEL |= 0x06; // Assign I2C pins to USCI_B3
	UCB3CTL1 |= UCSWRST; // Enable SW reset
	UCB3CTL0 = UCMST + UCMODE_3 + UCSYNC; // I2C Master, synchronous mode
	UCB3CTL1 = UCSSEL_2 + UCSWRST; // Use SMCLK, keep SW reset
	UCB3BR0 = 24; // fSCL = SMCLK/12 = ~100kHz	//yikim 12 -> 24
	UCB3BR1 = 0;
	UCB3I2CSA = slaveAddr; // Slave Address is 048h
	UCB3CTL1 &= ~UCSWRST; // Clear SW reset, resume operation
	UCB3IE |= UCRXIE; // Enable RX interrupt

	UCB3CTL1 |= UCTXSTT; // I2C start condition

	//wait
	int i = 0;
	int end = (len + 1) * 2;
	for (i = 0; i < end; i++) {
		MISC_delayUs(40); //  1byte : 40usec
		if (IsI2cComplete == TRUE)
			break;
	}
	//while (UCB3CTL1 & UCTXSTP);             // Ensure stop condition got sent

	if (IsI2cComplete == TRUE) {
		memcpy(data, RxBuffer, I2cRxByteLen);
		status = NTAG_RET_OK;
	} else {
		status = NTAG_RET_ERR;
		//printf("I2cRxPos :%d i:%d end:%d\n", I2cRxPos, i, end);
	}

	UCB3IE &= ~UCRXIE; // Disable RX interrupt

	return status;
}

BOOL I2C_txMultipleBytes(uint8 slaveAddr, uint8 *data, uint8 len)
{
	uint8 status = NTAG_RET_OK;

	if (len > MAX_I2C_TX_BUFF_SIZE) {
		len = MAX_I2C_TX_BUFF_SIZE;
	}

	memcpy(TxBuffer, data, len);
	I2cTxData = TxBuffer; // TX array start address
		// Place breakpoint here to see each
		// transmit operation.
	I2cTxByteCtr = len; // Load TX byte counter
	IsI2cComplete = FALSE;

	P10SEL |= 0x06; // Assign I2C pins to USCI_B3
	UCB3CTL1 |= UCSWRST; // Enable SW reset
	UCB3CTL0 = UCMST + UCMODE_3 + UCSYNC; // I2C Master, synchronous mode
	UCB3CTL1 = UCSSEL_2 + UCSWRST; // Use SMCLK, keep SW reset
	UCB3BR0 = 24; // fSCL = SMCLK/12 = ~100kHz	//yikim 12 -> 24
	UCB3BR1 = 0;
	UCB3I2CSA = slaveAddr; // Slave Address is 048h
	UCB3CTL1 &= ~UCSWRST; // Clear SW reset, resume operation
	UCB3IE |= UCTXIE; // Enable TX interrupt

	GLOBAL_ENABLE_INT();

	__delay_cycles(50); // Delay required between transaction

	UCB3CTL1 |= UCTR + UCTXSTT; // I2C TX, start condition

	//wait
	int i;
	int end = (len + 1) * 2;
	for (i = 0; i < end; i++) {
		MISC_delayUs(40); //  1byte : 40usec
		if (IsI2cComplete == TRUE)
			break;
	}
	//while (UCB3CTL1 & UCTXSTP);             // Ensure stop condition got sent

	if (IsI2cComplete == TRUE) {
		status = NTAG_RET_OK;
	} else {
		status = NTAG_RET_ERR;
		//printf("i2cTxByteCtr :%d i:%d end:%d\n", I2cTxByteCtr, i, end);
	}

	UCB3IE &= ~UCTXIE; // Disable TX interrupt

	return status;
}

//NFC I2C INTERRUPT
#pragma vector = USCI_B3_VECTOR
__interrupt void USCI_B3_ISR(void)
{
	int event = __even_in_range(UCB3IV, 12);
	switch (event) {
	case 0:
		break; // Vector  0: No interrupts
	case 2:
		break; // Vector  2: ALIFG
	case 4:
		break; // Vector  4: NACKIFG
	case 6:
		break; // Vector  6: STTIFG
	case 8:
		break; // Vector  8: STPIFG
	case 10: // Vector 10: RXIFG
		if (I2cRxPos < I2cRxByteLen) {
			RxBuffer[I2cRxPos] = UCB3RXBUF;
			I2cRxPos++;
			if (I2cRxPos == I2cRxByteLen - 1) {
				UCB3CTL1 |= UCTXSTP; // Generate I2C stop condition
				IsI2cComplete = TRUE;
			}
		} else {
			UCB3CTL1 |= UCTXSTP; // Generate I2C stop condition
			IsI2cComplete = TRUE;
		}
		break;
	case 12: // Vector 12: TXIFG
		if (I2cTxByteCtr) // Check TX byte counter
		{
			UCB3TXBUF = *I2cTxData++; // Load TX buffer
			I2cTxByteCtr--; // Decrement TX byte counter
		} else {
			UCB3CTL1 |= UCTXSTP; // I2C stop condition
			UCB3IFG &= ~UCTXIFG; // Clear USCI_B3 TX int flag
			IsI2cComplete = TRUE;
		}
		break;
	default:
		break;
	}
}

BOOL I2C_rxBytes(uint8 slaveAddr, uint8 *data, uint8 len)
{
	//오류시 2 회 실시
	for (int i = 0; i < 2; i++) {
		if (I2C_rxMultipleBytes(slaveAddr, data, len) == NTAG_RET_OK)
			return NTAG_RET_OK;
	}

	return NTAG_RET_ERR;
}

BOOL I2C_txBytes(uint8 slaveAddr, uint8 *data, uint8 len)
{
	//오류시 2 회 실시
	for (int i = 0; i < 2; i++) {
		if (I2C_txMultipleBytes(slaveAddr, data, len) == NTAG_RET_OK)
			return NTAG_RET_OK;
	}
	return NTAG_RET_ERR;
}

BOOL I2C_writeData(uint8 addr, uint8 *data, uint8 len)
{
	uint8 txBuff[MAX_I2C_TX_BUFF_SIZE];

	txBuff[0] = addr;
	memcpy(&txBuff[1], data, len);

	if (I2C_txBytes(I2C_DEVICE_ADDR, txBuff, len + 1) == NTAG_RET_ERR)
		return NTAG_RET_ERR;

	return NTAG_RET_OK;
}

BOOL I2C_readData(uint8 addr, uint8 *data, uint8 len)
{
	uint8 txBuff[2];

	txBuff[0] = addr;

	if (I2C_txBytes(I2C_DEVICE_ADDR, txBuff, 1) == NTAG_RET_ERR)
		return NTAG_RET_ERR;

	if (I2C_rxBytes(I2C_DEVICE_ADDR, data, len) == NTAG_RET_ERR)
		return NTAG_RET_ERR;

	return NTAG_RET_OK;
}

BOOL NTAG_readSessionRegister(uint8 regAddr, uint8 *regData)
{
	uint8 txBuff[2];

	txBuff[0] = NTAG_MEM_BLOCK_SESSION_REGS;
	txBuff[1] = regAddr;

	if (I2C_txBytes(I2C_DEVICE_ADDR, txBuff, 2) == NTAG_RET_ERR)
		return NTAG_RET_ERR;

	if (I2C_rxBytes(I2C_DEVICE_ADDR, regData, 1) == NTAG_RET_ERR)
		return NTAG_RET_ERR;

	return NTAG_RET_OK;
}

BOOL NTAG_writeSessionRegister(uint8 regAddr, uint8 mask, uint8 regData)
{
	uint8 txBuff[4];

	txBuff[0] = NTAG_MEM_BLOCK_SESSION_REGS;
	txBuff[1] = regAddr;
	txBuff[2] = mask;
	txBuff[3] = regData;

	if (I2C_txBytes(I2C_DEVICE_ADDR, txBuff, 4) == NTAG_RET_OK)
		return NTAG_RET_OK;

	return NTAG_RET_ERR;
}

BOOL NTAG_readBlock(uint8 addr, uint8 *bytes, uint8 len)
{
	/* receive bytes */
	if (I2C_readData(addr, bytes, NTAG_I2C_BLOCK_SIZE) == NTAG_RET_ERR) {
		return NTAG_RET_ERR;
	}

	return NTAG_RET_OK;
}

BOOL NTAG_writeBlock(uint8 addr, const uint8 *bytes, uint8 len)
{
	uint8 nsReg = 0;
	uint32 timeout = NTAG_MAX_WRITE_DELAY_MS / 5 + 1;
	int i = 0;
	uint8 status;

	uint8 txBuffer[NTAG_I2C_BLOCK_SIZE];

	len = MIN(len, NTAG_I2C_BLOCK_SIZE);

	/* copy len bytes */
	for (i = 0; i < len; i++)
		txBuffer[i] = bytes[i];

	/* zero rest of the buffer */
	for (i = len; i < NTAG_I2C_BLOCK_SIZE; i++)
		txBuffer[i] = 0;

	/* send block number */
	status = I2C_writeData(addr, txBuffer, NTAG_I2C_BLOCK_SIZE);
	if (status == NTAG_RET_ERR) {
		return NTAG_RET_ERR;
	}

	/* do not wait for completion when writing SRAM */
	if (addr >= NTAG_MEM_BLOCK_START_SRAM &&
	    addr < NTAG_MEM_BLOCK_START_SRAM + NTAG_MEM_SRAM_BLOCKS)
		return NTAG_RET_OK;

	/* wait for completion */
	do {
		MISC_delayMs(5);
		if (NTAG_readSessionRegister(NTAG_MEM_OFFSET_NS_REG, &nsReg) == NTAG_RET_ERR)
			break;
		timeout--;
	} while (timeout && nsReg & NTAG_NS_REG_MASK_EEPROM_WR_BUSY);

	if (0 == timeout)
		return NTAG_RET_ERR; //ntag->status = NTAG_ERROR_WRITE_TIMEOUT;

	return NTAG_RET_OK;
}

BOOL NTAG_readBytes(uint8 startAddr, uint8 *bytes, uint16 len)
{
	uint16 bytesRead = 0;
	uint8 rxBuff[NTAG_I2C_BLOCK_SIZE];
	BOOL status = NTAG_RET_OK;

	while (bytesRead < len) {
		uint8 currentAddr = startAddr + bytesRead / NTAG_I2C_BLOCK_SIZE;
		uint8 begin = bytesRead % NTAG_I2C_BLOCK_SIZE;
		uint8 currentLen = MIN(len - bytesRead, NTAG_I2C_BLOCK_SIZE - begin);

		if (currentLen < NTAG_I2C_BLOCK_SIZE) {
			int i = 0;

			memset(rxBuff, 0, NTAG_I2C_BLOCK_SIZE);

			/* read block into ntag->rxBuffer only */
			status = NTAG_readBlock(currentAddr, rxBuff, currentLen);
			if (status == NTAG_RET_ERR)
				break;

			/* modify rxBuffer */
			for (i = 0; i < currentLen; i++)
				bytes[bytesRead + i] = rxBuff[i];
		} else {
			/* full block read */
			status =
				NTAG_readBlock(currentAddr, &bytes[bytesRead], NTAG_I2C_BLOCK_SIZE);
			if (status == NTAG_RET_ERR)
				break;
		}

		bytesRead += currentLen;
	}
	return status;
}

BOOL NTAG_writeBytes(uint8 startAddr, const uint8 *bytes, uint16 len)
{
	uint16 bytesWritten = 0;
	uint8 rxBuffer[NTAG_I2C_BLOCK_SIZE];
	BOOL status = NTAG_RET_OK;

	while (bytesWritten < len) {
		uint8 currentAddr = startAddr + bytesWritten / NTAG_I2C_BLOCK_SIZE;
		uint8 begin = bytesWritten % NTAG_I2C_BLOCK_SIZE;
		uint8 currentLen = MIN(len - bytesWritten, NTAG_I2C_BLOCK_SIZE - begin);

		if (currentLen < NTAG_I2C_BLOCK_SIZE) {
			int i = 0;

			/* read block into ntag->rxBuffer only */
			status = NTAG_readBlock(currentAddr, rxBuffer, NTAG_I2C_BLOCK_SIZE);
			if (status == NTAG_RET_ERR)
				break;

			/* modify rxBuffer */
			for (i = 0; i < currentLen; i++)
				rxBuffer[i] = bytes[bytesWritten + i];

			/* writeback modified buffer */
			status = NTAG_writeBlock(currentAddr, rxBuffer, NTAG_I2C_BLOCK_SIZE);
			if (status == NTAG_RET_ERR)
				break;
		} else {
			/* full block write */
			status = NTAG_writeBlock(currentAddr, &bytes[bytesWritten],
						 NTAG_I2C_BLOCK_SIZE);
			if (status == NTAG_RET_ERR)
				break;
		}

		bytesWritten += currentLen;
	}

	return status;
}

//---------------------------------------------------------------------
BOOL NTAG_setFDOnFunction(uint8 func)
{
	return NTAG_writeSessionRegister(NTAG_MEM_OFFSET_NC_REG, NTAG_NC_REG_MASK_FD_ON, func);
}

//---------------------------------------------------------------------
BOOL NTAG_setFDOffFunction(uint8 func)
{
	return NTAG_writeSessionRegister(NTAG_MEM_OFFSET_NC_REG, NTAG_NC_REG_MASK_FD_OFF, func);
}

//---------------------------------------------------------------------
BOOL NTAG_setPthruOnOff(BOOL on)
{
	uint8 val = 0;
	if (on)
		val = NTAG_NC_REG_MASK_PTHRU_ON_OFF;
	else
		val = 0;

	return NTAG_writeSessionRegister(NTAG_MEM_OFFSET_NC_REG, NTAG_NC_REG_MASK_PTHRU_ON_OFF,
					 val);
}

//---------------------------------------------------------------------
BOOL NTAG_setTransferDir(uint8 dir)
{
	BOOL err = NTAG_RET_OK;
	uint8 currentSesReg = 0;
	NTAG_readSessionRegister(NTAG_MEM_OFFSET_NC_REG, &currentSesReg);

	if ((currentSesReg & NTAG_NC_REG_MASK_TRANSFER_DIR) != dir) {
		if (currentSesReg & NTAG_NC_REG_MASK_PTHRU_ON_OFF) {
			NTAG_setPthruOnOff(FALSE);
			err = NTAG_writeSessionRegister(NTAG_MEM_OFFSET_NC_REG,
							NTAG_NC_REG_MASK_TRANSFER_DIR, dir);
			NTAG_setPthruOnOff(TRUE);
		} else {
			err = NTAG_writeSessionRegister(NTAG_MEM_OFFSET_NC_REG,
							NTAG_NC_REG_MASK_TRANSFER_DIR, dir);
		}
	}
	// already set do nothing
	return err;
}

void NFC_tagEnable()
{
	//NFC TAG
	PORT1_DIR &= ~BM(PORT_NFC_TAG); // Set to Input
	PORT1_REN &= ~BM(PORT_NFC_TAG); //Input

	PORT1_IES &= ~BM(PORT_NFC_TAG); // Low ->Hi edge
	PORT1_IFG &= ~BM(PORT_NFC_TAG); // IFG cleared
	PORT1_IE |= BM(PORT_NFC_TAG); // Interrupt enabled
}

void NFC_tagDisable()
{
	PORT1_IFG &= ~BM(PORT_NFC_TAG); // IFG cleared
	PORT1_IE &= ~BM(PORT_NFC_TAG); // Interrupt diabled
}

void NFC_fdEnable()
{
	//FD
	PORT1_DIR &= ~BM(PORT_NFC_FD_IN); // Set to Input
	PORT1_REN |= BM(PORT_NFC_FD_IN); //pullup
	PORT1_OUT |= BM(PORT_NFC_FD_IN); //pullup

	PORT1_IES |= BM(PORT_NFC_FD_IN); // Hi -> Low edge
	PORT1_IFG &= ~BM(PORT_NFC_FD_IN); // IFG cleared
	PORT1_IE |= BM(PORT_NFC_FD_IN); // Interrupt enabled
}

void NFC_fdDisable()
{
	PORT1_IFG &= ~BM(PORT_NFC_FD_IN); // IFG cleared
	PORT1_IE &= ~BM(PORT_NFC_FD_IN); // Interrupt disabled
}

BOOL NFC_FD_input()
{
	uint8 input = PORT1_IN & BM(PORT_NFC_FD_IN);

	if (input > 0)
		return TRUE;
	else
		return FALSE;
}

// NFC read event
// Check FD high -> low
//
//   ------|
//             |________
BOOL NFC_checkRead(int msec)
{
	BOOL prevFd = FALSE, currentFd = TRUE;
	uint32 count = msec * 99;
	while (count--) {
		currentFd = NFC_FD_input();
		prevFd = prevFd ? prevFd : currentFd;
		if (prevFd == TRUE && currentFd == FALSE) {
			break;
		}
		MISC_delayUs(10);
	}
	return (count > 0) ? TRUE : FALSE;
}

void NFC_init()
{
	NFC_tagEnable();
	NFC_fdDisable();
}

BOOL NFC_tagDetect()
{
	PRINT_stop();
	NFC_tagDisable();
	NFC_fdDisable();

	uint8 nStatus = 0;
	nStatus += NTAG_setFDOffFunction(I2C_LAST_DATA_READ_OR_WRITTEN_OR_RF_SWITCHED_OFF_11b);
	nStatus += NTAG_setFDOnFunction(DATA_READY_BY_I2C_OR_DATA_READ_BY_RF_11b);

	nStatus += NTAG_setTransferDir(RF_TO_I2C);
	nStatus += NTAG_setPthruOnOff(TRUE);

	NFC_tagEnable();
	NFC_fdEnable();
	PRINT_resume();

	return (nStatus <= 0);
}

BOOL NFC_recvMessage()
{
	PRINT_stop();
	NFC_tagDisable();
	NFC_fdDisable();

	uint8 msg[64];
	memset(msg, 0, 64);
	int nStatus = NTAG_readBytes(NTAG_MEM_BLOCK_START_SRAM, msg, 64);

	NfcMsg_t *p = (NfcMsg_t *)msg;
	int len = MSG_OFFSET_LEN + p->header.len;

	uint8 cChecksum = 0;
	for (int i = 0; i < len; i++) {
		cChecksum += msg[i];
	}
	uint8 rChecksum = msg[len];

	if (nStatus == 0 && p->header.deviceCode == DEVICE_CODE_SMART_PHONE &&
	    cChecksum == rChecksum) {
		NFCAPP_runMessage(msg, len);
	}

	NFC_tagEnable();
	NFC_fdEnable();
	PRINT_resume();

	return (nStatus <= 0);
}

BOOL NFC_sendMessage(byte *msg, uint8 len)
{
	PRINT_stop();
	NFC_tagDisable();
	NFC_fdDisable();

	uint8 buff[64];
	memset(buff, 0, 64);

	uint8 checksum = 0;
	for (int i = 0; i < len; i++) {
		buff[i] = msg[i];
		checksum += msg[i];
	}
	buff[len] = checksum;

	int nStatus = NTAG_setPthruOnOff(TRUE);
	nStatus += NTAG_setTransferDir(I2C_TO_RF);

	nStatus += NTAG_writeBytes(NTAG_MEM_BLOCK_START_SRAM, buff, 64);

	nStatus += NTAG_setFDOffFunction(I2C_LAST_DATA_READ_OR_WRITTEN_OR_RF_SWITCHED_OFF_11b);
	nStatus += NTAG_setFDOnFunction(DATA_READY_BY_I2C_OR_DATA_READ_BY_RF_11b);

	//nStatus += NTAG_setTransferDir(RF_TO_I2C);
	//nStatus += NTAG_setPthruOnOff(TRUE);

	NFC_tagEnable();
	NFC_fdEnable();
	PRINT_resume();

	return (nStatus <= 0);
}

BOOL NFC_factoryResetTag()
{
	MISC_delayMs(100);

	int nStatus = NTAG_setTransferDir(I2C_TO_RF);
	nStatus += NTAG_setPthruOnOff(FALSE);

	uint8 page = 1;
	while (page <= 7) {
		nStatus += NTAG_writeBlock(page, NullBlock, NTAG_I2C_BLOCK_SIZE);
		page++;
	}

	//reset default eeprom memory values (smart poster)
	nStatus += NTAG_writeBytes(NTAG_MEM_ADRR_I2C_ADDRESS, DefaultBeginingOfMemory,
				   DefaultBeginingOfMemoryLength);

	//reset pages from 8 to 56
	page = 8;
	while (page < 56) {
		nStatus += NTAG_writeBlock(page, NullBlock, NTAG_I2C_BLOCK_SIZE);
		page++;
	}
	//reset pages 56,57,58
	nStatus += NTAG_writeBlock(56, DefaultPage56, NTAG_I2C_BLOCK_SIZE);
	nStatus += NTAG_writeBlock(57, DefaultPage57, NTAG_I2C_BLOCK_SIZE);
	nStatus += NTAG_writeBlock(58, DefaultPage58, NTAG_I2C_BLOCK_SIZE);

	MISC_delayMs(100);

	return (nStatus <= 0);
}

BOOL NFC_checkTagSetting()
{
#define TAG_SETTING_CMP_POS 11
	uint8 *tagSettings = (uint8 *)malloc(sizeof(uint8) * DefaultBeginingOfMemoryLength);
	memset(tagSettings, 0, DefaultBeginingOfMemoryLength);

	MISC_delayMs(100);
	int nStatus = NTAG_readBytes(NTAG_MEM_ADRR_I2C_ADDRESS, tagSettings,
				     DefaultBeginingOfMemoryLength);
	MISC_delayMs(100);

	BOOL result = FALSE;
	if (nStatus <= 0) {
		if (memcmp((tagSettings + TAG_SETTING_CMP_POS),
			   (DefaultBeginingOfMemory + TAG_SETTING_CMP_POS),
			   (DefaultBeginingOfMemoryLength - TAG_SETTING_CMP_POS))) {
			result = FALSE;
		} else {
			result = TRUE;
		}
	}
	free(tagSettings);

	return result;
}

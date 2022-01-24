#ifndef CC1200_RX_SNIFF_MODE_TX
#define CC1200_RX_SNIFF_MODE_TX

#define CC1200_GPIO3 0x01 // Not use
#define CC1200_GPIO0 0x80 // Interrupt input pin

#define CC1200_RESET 0x01 // P8.0

#define TCXO_BIT 0x80 // Port1 bit 7
#define TURN_ON_TCXO                                                                               \
	P1DIR |= TCXO_BIT;                                                                         \
	P1OUT &= ~TCXO_BIT;
#define TURN_OFF_TCXO                                                                              \
	P1DIR |= TCXO_BIT;                                                                         \
	P1OUT |= TCXO_BIT;

#define RF_MODE_IDLE 0
#define RF_MODE_TX 1
#define RF_MODE_RX 2

#define LONG_PREAMBLE 1
#define SHORT_PREAMBLE 0

#define WOR_LONG_DUTY 1
#define WOR_SHORT_DUTY 0

#define MIN_LEN_RF_BUF 22 //sizeof(RfMessageHeader_t) + crc 2
#define LEN_RF_BUF 0x90 //144byte
#define NUM_RF_RX_BUF 3

#define RADIO_EVENT_RF_TX 0x00000001
#define RADIO_EVENT_RF_RX 0x00000002
#define RADIO_MSG_CHECK 0x00000004

#define NWK_HDR_FLAG_DATA_REQ 0x01
#define NWK_HDR_FLAG_ACK 0x02
#define NWK_HDR_FLAG_DATA_RESP 0x04

#define CC1200_RSSI_OFFSET -99 // should be confirmed later (refer to 6.9 of user's guide)
#define RX_POWER_LIMITED_VAL 61 //Rx Power limited Value //-61dBm
#define CC1200_RETX_TIMEOUT 1500 // 1홉에 대한 타임아웃(1.5sec)
#define CC1200_LONG_TIMEOUT 4000 // Slave Meter MT Down 시간 고려

#define INFINITE_PACKET_LENGTH_MODE 0x40
#define FIXED_PACKET_LENGTH_MODE 0x00
#define FIFO_SIZE 128
#define AVAILABLE_BYTES_IN_TX_FIFO                                                                 \
	122 // # of bytes one can write to the                                 \
		// TX_FIFO when a falling edge occur                           \
		// on IOCFGx = 0x02 and                                        \
		// FIFO_THR = 120

//송신 메시지에 대한 응답을 기다리는 대기시간 (디바이스가 바로 Sleep Mode로 들어가  송신메시지에 대한 응답 메시지(Ack가 아닌)를 받지 못하는 문제를 방지한다.)
#define NWK_RX_WAIT_TIMEOUT(D) (1500 * ((D == 0) ? 1 : D)) * 2 // *2 는 왕복

// Start of delimeter
#define SYNC_BYTE 0xD3
#define NSYNC_BYTE 0x91

// message definition
typedef struct {
	uint8 pan[2];
	uint8 nwk[4];
	uint8 mLen[2];
	uint8 srcAddr[4];
	uint8 destAddr[4];
	uint8 flag;
	uint8 seq;
	uint8 ack;
	uint8 nLen;
} RfMessageHeader_t;

#define FALLING_EDGE 0
#define RISING_EDGE 1

#define SET_CC1200_INTERRUPT(dir)                                                                  \
	do {                                                                                       \
		P2IE &= ~CC1200_GPIO0;                                                             \
		if (dir == FALLING_EDGE) {                                                         \
			P2IES |= CC1200_GPIO0;                                                     \
		} else {                                                                           \
			P2IES &= ~CC1200_GPIO0;                                                    \
		}                                                                                  \
		P2IFG &= ~CC1200_GPIO0;                                                            \
		P2IE |= CC1200_GPIO0;                                                              \
	} while (0);

#define DISABLE_CC1200_INTERRUPT()                                                                 \
	do {                                                                                       \
		P2IE &= ~CC1200_GPIO0;                                                             \
		P2IFG &= ~CC1200_GPIO0;                                                            \
	} while (0);

// function proto-type
void CC1200_init(uint8 taskId);
void CC1200_setReady();
void CC1200_setSleep();

event32_t CC1200_tasks(uint8 taskId, event32_t events);

void CC1200_reset();
void CC1200_open(int ch);
void CC1200_idle();
void CC1200_cal();
void CC1200_changeChannel(uint8 ch);
void CC1200_setChannel(uint8 ch);
int CC1200_getChannel();
int8 CC1200_readRSSI();
BOOL CC1200_readPreambleDetect();

void CC1200_setRxMode();
void CC1200_wor();
void CC1200_Rx();
void CC1200_worDuty(int duty);
int CC1200_getRxLen();
uint8 CC1200_getRfMode();
void CC1200_clearRxBuf();
void CC1200_setOutputPower(int level);

void CC1200_rfOff();
void CC1200_rfOn();

void CC1200_syncWordEnable();
void CC1200_preambleEnable();
void CC1200_startTx();
void CC1200_setTxMode();
void CC1200_hwReset();
void CC1200_sendMessage2RF(uint8 *msg, int len, int preamble);

BOOL CC1200_isRadioTxDone();
void CC1200_isr();
void CC1200_read_rxByte();

BOOL CC1200_dataRequest(uint32 destAddr, uint8 ackFlag, uint8 retryCnt, byte *msg, uint8 len);
BOOL CC1200_testGetReceivedData(uint32 *pTime, uint8 *data, uint8 *pLen, uint8 *pRssi, BOOL *pCRC);
uint32 CC1200_getParentAddr(uint32 addr);
void CC1200_calCRC(uint8 *data, int len, uint16 *crc);
uint8 CC1200_getRxPower(int8 rs);
uint8 CC1200_getDefaultChannel();
#endif

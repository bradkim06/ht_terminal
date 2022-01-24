#include <ctype.h>

#include "common_header.h"

#include "port_desc.h"
#include "check_meter_misc.h"
#include "osal_Timer.h"
#include "Task_Mgr.h"
#include "shell.h"
#include "test.h"
#include "flashDriver.h"
#include "meter.h"
#include "cc1200.h"
#include "rfTest.h"

extern Config_t conf;
static uint8 TestRxBuffer[0x128];

static char *getMsgName(uint8 msgType)
{
	char *p = "UNKNOWN_MESSAGE";

	switch (msgType) {
	case MSG_NODE_JOIN_NOTIFY:
		p = "MSG_NODE_JOIN_NOTIFY";
		break;
	case MSG_AMI_TIME_SYNC_REQ:
		p = "MSG_AMI_TIME_SYNC_REQ";
		break;
	case MSG_AMI_TIME_SYNC_SET:
		p = "MSG_AMI_TIME_SYNC_SET";
		break;
	case MSG_AMI_NODE_EVENT_ALARM:
		p = "MSG_AMI_NODE_EVENT_ALARM";
		break;
	case MSG_AMI_NODE_STATUS_REQ:
		p = "MSG_AMI_NODE_STATUS_REQ";
		break;
	case MSG_AMI_NODE_STATUS_REPORT:
		p = "MSG_AMI_NODE_STATUS_REPORT";
		break;
	case MSG_AMI_METER_CONF_SET:
		p = "MSG_AMI_METER_CONF_SET";
		break;
	case MSG_AMI_DATA_REPORT:
		p = "MSG_AMI_DATA_REPORT";
		break;
	case MSG_PDA_DATA_REQ:
		p = "MSG_PDA_DATA_REQ";
		break;
	case MSG_PDA_DATA_REPORT:
		p = "MSG_PDA_DATA_REPORT";
		break;
	case MSG_PDA_ONE_DATA_REPORT:
		p = "MSG_PDA_ONE_DATA_REPORT";
		break;
	case MSG_PDA_DATA_REQ_FINISH:
		p = "MSG_PDA_DATA_REQ_FINISH";
		break;
	case MSG_PDA_NODE_CONF_SET:
		p = "MSG_PDA_NODE_CONF_SET";
		break;
	case MSG_PDA_NODE_CONF_SUCCESS:
		p = "MSG_PDA_NODE_CONF_SUCCESS";
		break;
	case MSG_PDA_NODE_CONF_REQ:
		p = "MSG_PDA_NODE_CONF_REQ";
		break;
	case MSG_PDA_NODE_CONF_REPORT:
		p = "MSG_PDA_NODE_CONF_REPORT";
		break;
	case MSG_NODE_TIME_REQ:
		p = "MSG_NODE_TIME_REQ";
		break;
	case MSG_NODE_TIME_REPORT:
		p = "MSG_NODE_TIME_REPORT";
		break;
	case MSG_PDA_GROUP_DATA_REPORT:
		p = "MSG_PDA_GROUP_DATA_REPORT";
		break;
	case MSG_PDA_PERIOD_DATA_REPORT:
		p = "MSG_PDA_PERIOD_DATA_REPORT";
		break;
	case MSG_PDA_NODE_RXPOWER_REQ:
		p = "MSG_PDA_NODE_RXPOWER_REQ";
		break;
	case MSG_PDA_NODE_RXPOWER_REPORT:
		p = "MSG_PDA_NODE_RXPOWER_REPORT";
		break;
	case MSG_PDA_GROUP_SCAN_REQ:
		p = "MSG_PDA_GROUP_SCAN_REQ";
		break;
	case MSG_PDA_MULTI_DATA_REPORT:
		p = "MSG_PDA_MULTI_DATA_REPORT";
		break;
	case MSG_AMI_NODE_JOIN_ACK:
		p = "MSG_AMI_NODE_JOIN_ACK";
		break;
	case MSG_AMI_NODE_STATUS_ACK:
		p = "MSG_AMI_NODE_STATUS_ACK";
		break;
	case MSG_AMI_MULTI_DATA_REPORT:
		p = "MSG_AMI_MULTI_DATA_REPORT";
		break;
	case MSG_AMI_DATA_ACK:
		p = "MSG_AMI_DATA_ACK";
		break;
	case MSG_PDA_PULSE_VALUE_SET:
		p = "MSG_PDA_PULSE_VALUE_SET";
		break;
	case MSG_PDA_PULSE_VALUE_SUCCESS:
		p = "MSG_PDA_PULSE_VALUE_SUCCESS";
		break;
	case MSG_AMI_SLAVE_DATA_REPORT:
		p = "MSG_AMI_SLAVE_DATA_REPORT";
		break;
	case MSG_AMI_NODE_INFO_REPORT:
		p = "MSG_AMI_NODE_INFO_REPORT";
		break;
	case MSG_AMI_NODE_MULTI_INFO_REPORT:
		p = "MSG_AMI_NODE_MULTI_INFO_REPORT";
		break;
	case MSG_AMI_MULTI_METER_REPORT:
		p = "MSG_AMI_MULTI_METER_REPORT";
		break;
	case MSG_PDA_PERIOD_MULTI_DATA_REPORT:
		p = "MSG_PDA_PERIOD_MULTI_DATA_REPORT";
		break;
	case MSG_AMI_SLAVE_NODE_JOIN_NOTIFY:
		p = "MSG_AMI_SLAVE_NODE_JOIN_NOTIFY";
		break;
	case MSG_AMI_MASTER_SLAVE_METER_REQ:
		p = "MSG_AMI_MASTER_SLAVE_METER_REQ";
		break;
	case MSG_AMI_MASTER_SLAVE_STATUS_REQ:
		p = "MSG_AMI_MASTER_SLAVE_STATUS_REQ";
		break;
	case MSG_AMI_MASTER_SLAVE_STATUS_REPORT:
		p = "MSG_AMI_MASTER_SLAVE_STATUS_REPORT";
		break;
	case MSG_PDA_SLAVE_CHECK_REQ:
		p = "MSG_PDA_SLAVE_CHECK_REQ";
		break;
	case MSG_PDA_SLAVE_CHECK_REPORT:
		p = "MSG_PDA_SLAVE_CHECK_REPORT";
		break;
	case MSG_MASTER_SLAVE_CHECK_REQ:
		p = "MSG_MASTER_SLAVE_CHECK_REQ";
		break;
	case MSG_MASTER_SLAVE_CHECK_REPORT:
		p = "MSG_MASTER_SLAVE_CHECK_REPORT";
		break;
	case MSG_PDA_MASTER_SLAVE_METER_REQ:
		p = "MSG_PDA_MASTER_SLAVE_METER_REQ";
		break;
	case MSG_AMI_SLAVE_NODE_EVENT_ALARM:
		p = "MSG_AMI_SLAVE_NODE_EVENT_ALARM";
		break;
	case MSG_NODE_FW_VER_REPORT:
		p = "MSG_NODE_FW_VER_REPORT";
		break;
	case MSG_TEST_NODE_TX_REQ:
		p = "MSG_TEST_NODE_TX_REQ";
		break;
	case MSG_TEST_NODE_TX_ACK:
		p = "MSG_TEST_NODE_TX_ACK";
		break;
	case MSG_METER_DATA_ERASE_REQ:
		p = "MSG_METER_DATA_ERASE_REQ";
		break;
	case MSG_METER_DATA_ERASE_ACK:
		p = "MSG_METER_DATA_ERASE_ACK";
		break;
	case MSG_PDA_E_METER_DATA_REQ:
		p = "MSG_PDA_E_METER_DATA_REQ";
		break;
	case MSG_PDA_E_METER_DATA_REPORT:
		p = "MSG_PDA_E_METER_DATA_REPORT";
		break;
	case MSG_PDA_FREQ_BAND_SET:
		p = "MSG_PDA_FREQ_BAND_SET";
		break;
	case MSG_PDA_FREQ_BAND_SUCCESS:
		p = "MSG_PDA_FREQ_BAND_SUCCESS";
		break;
	case MSG_AMI_E_METER_DATA_REPORT:
		p = "MSG_AMI_E_METER_DATA_REPORT";
		break;
	default:
		break;
	}
	return p;
}

static void displayMsgHeader(uint32 rxtime, uint8 mtype, uint8 rssi, uint16 panid, char *src,
			     char *dest, char *next, uint8 flag, uint8 seq, uint8 ack, uint8 len)
{
	uint32 hour = 0;
	uint32 min = 0;
	uint32 sec = rxtime / 1000;
	uint32 ms = rxtime % 1000;

	hour = sec / 3600;
	if (hour > 23) {
		hour %= hour;
	}
	min = (sec % 3600) / 60;
	sec = sec % 60;

	char *pMsgName = "ACK";
	if (len) {
		pMsgName = getMsgName(mtype);
	}

	printf("\n");
	printf("%s  [%02d:%02d:%02d.%03d]  RSSI: %3d\n", pMsgName, (uint8)hour, (uint8)min,
	       (uint8)sec, (uint16)ms, rssi);

	printf("   Pan: %04x Src: %s Dest: %s Next: %s\n", panid, src, dest, next);
	printf("   Flag: %d Seq: %d Ack: %d Len: %d\n", flag, seq, ack, len);
}

static void makeString(char *p, uint32 nwkID)
{
	sprintf(p, "%d.%d.%d.%d", (uint8)((nwkID >> 24) & 0xff), (uint8)((nwkID >> 16) & 0xff),
		(uint8)((nwkID >> 8) & 0xff), (uint8)(nwkID & 0xff));
}

static void displayMessage(uint32 rx_time, uint8 *payload, uint8 rxlen, uint8 rssi)
{
	char strNextAddr[0x10] = "";
	char strDestAddr[0x10] = "";
	char strSrcAddr[0x10] = "";
	uint16 panid = BUILD_UINT16(payload[0], payload[1]);
	uint32 temp;
	memcpy(&temp, payload + 2, 4);
	makeString(strNextAddr, temp);
	// macHeader Len 2byte

	memcpy(&temp, payload + 8, 4);
	makeString(strSrcAddr, temp);

	memcpy(&temp, payload + 12, 4);
	makeString(strDestAddr, temp);

	uint8 flag = *(payload + 16);
	uint8 seq = *(payload + 17);
	uint8 ack = *(payload + 18);
	uint8 len = *(payload + 19);
	uint8 mtype = *(payload + 20);
	displayMsgHeader(rx_time, mtype, rssi, panid, strSrcAddr, strDestAddr, strNextAddr, flag,
			 seq, ack, len);

	printf("     ");
	for (int i = 0; i < len; i++) {
		if (i && !(i % 16)) {
			printf("\n     ");
		}
		printf("%02x ", *(payload + i + 20));
	}
	printf("\n");
}

static uint16 getPanId(char *pPanid)
{
	uint16 panid = 0;
	uint8 *p = (uint8 *)&panid;
	uint8 upperNibble = 0;

	for (int i = 0; i < 4; i++) {
		uint8 id = *(pPanid + i);
		if (isdigit(id)) {
			id -= '0';
		} else {
			id = toupper(id);
			id = id - 'A' + 0xa;
		}
		if ((i % 2)) {
			uint8 temp = upperNibble * 16 + id;
			*(p + (3 - i) / 2) = temp;
		} else {
			upperNibble = id;
		}
	}

	return panid;
}

static uint32 getNwkId(char *pNwkid)
{
	uint32 nwkid = 0;

	uint8 *p = (uint8 *)&nwkid;
	p += 3;
	uint8 digit = 0;

	while (*pNwkid) {
		if (*pNwkid == '.') {
			*p = digit;
			digit = 0;
			p--;
		} else {
			digit *= 10;
			digit += (*pNwkid - '0');
		}
		pNwkid++;
	}
	*p = digit;

	return nwkid;
}

void TEST_readIDs()
{
	Config_t config;
	FLASH_readConfigInfo(&config);

	printf(" PAN ID     : %04X\n", config.pan_id);

	uint8 *p = (uint8 *)&config.nwk_addr;
	printf(" NWK        : %d.%d.%d.%d\n", *(p + 3), *(p + 2), *(p + 1), *(p + 0));

	p = (uint8 *)&config.slaveNwk;
	printf(" Slave NWK  : %d.%d.%d.%d\n", *(p + 3), *(p + 2), *(p + 1), *(p + 0));
}

void TEST_writeIDs(char *pPanid, char *pNwkid, int slaveID)
{
	BOOL bValid = TRUE;

	if (strlen(pPanid) != 4) {
		bValid = FALSE;
	}

	for (int i = 0; i < strlen(pPanid) && bValid; i++) {
		uint8 temp = *(pPanid + i);
		if (isxdigit(temp) == 0) {
			bValid = FALSE;
			break;
		}
	}

	int nDot = 0;

	for (int i = 0; i < strlen(pNwkid) && bValid; i++) {
		uint8 temp = *(pNwkid + i);
		if (isdigit(temp) == 0 && temp != '.') {
			bValid = FALSE;
			break;
		}
		if (temp == '.')
			nDot++;
	}

	if (bValid == FALSE || nDot != 3) {
		printf("invalid panid(%s) or nwkid(%s)\n", pPanid, pNwkid);
		return;
	}

	Config_t config;
	FLASH_readConfigInfo(&config);

	uint16 panid = getPanId(pPanid);
	memcpy(&config.pan_id, &panid, 2);

	uint32 nwkid = getNwkId(pNwkid);
	memcpy(&config.nwk_addr, &nwkid, 4);

	int zeroPos = getFirstZeroPosition(config.nwk_addr);
	uint8 addr[4];
	memcpy(addr, &config.nwk_addr, 4);
	addr[zeroPos] = slaveID;
	memcpy(&config.slaveNwk, addr, 4);

	FLASH_saveConfigInfo(&config);
	TEST_readIDs();
}

void TEST_ContinuousTx(int ch, int txLevel, BOOL zigMsg)
{
	if (zigMsg) {
		printf("ACK\n");
	} else {
		printf("Continuous tx : ch = %d\n", ch);
	}

	FLASH_readConfigInfo(&conf);

	const int FREQ_OFFSET_STEP = 3;
	int freqOffset = conf.freqOffset;
	int power = (txLevel >= 0 && txLevel <= 2) ? txLevel : 0;

	CC1200_rfOn();
	CC1200_setReady();
	CC1200_open(ch);
	CC1200_setOutputPower(txLevel);
	CC1200_setTxMode();
	CC1200_preambleEnable();
	CC1200_startTx();

#if 1
	while (1) {
		uint8 c = SHELL_getChar();
		if (c == 'q' || c == 'Q') {
			break;
		}

		if (c == CHAR_LEFT_ARROW || c == CHAR_RIGHT_ARROW || c == CHAR_UP_ARROW ||
		    c == CHAR_DOWN_ARROW) {
			int f_changed = 0;
			switch (c) {
			case CHAR_LEFT_ARROW:
				freqOffset -= FREQ_OFFSET_STEP;
				f_changed = 1;
#if 0
                        if(zigMsg){
                            printf("toffset %d\n",freqOffset);
                        }
#endif
				break;

			case CHAR_RIGHT_ARROW:
				freqOffset += FREQ_OFFSET_STEP;
				f_changed = 1;
#if 0
                        if(zigMsg){
                            printf("toffset %d\n",freqOffset);
                        }
#endif
				break;

			case CHAR_UP_ARROW:
				if (--power < 0) {
					power = 0;
				}
				break;

			case CHAR_DOWN_ARROW:
				if (++power > 2) {
					power = 0;
				}
				break;
			}

			if (f_changed) {
				conf.freqOffset = freqOffset;
				FLASH_saveConfigInfo(&conf);
			}

			CC1200_idle();
			CC1200_open(ch);
			CC1200_setOutputPower(power);
			CC1200_setTxMode();
			CC1200_preambleEnable();
			CC1200_startTx();

			printf("ch:%d freqOffset:%d power:%d\n", ch, freqOffset, power);
		}
	}
#else
	BOOL bActive = TRUE;
	while (bActive) {
		if (TEST_stop() == TRUE) {
			bActive = FALSE;
			break;
		}
	}
#endif

	CC1200_setRxMode();
	CC1200_worDuty(WOR_SHORT_DUTY);
	CC1200_wor();
}

void TEST_TxData(int ch, int txLevel, int mlen, BOOL zigMsg)
{
	if (zigMsg) {
		printf("ACK\n");
	} else {
		printf("[Test_TxData] ch:%d len:%d\n", ch, mlen);
	}

	int nMessage = 0;
	uint8 payload[0x80];

	if (mlen >= 0x80) {
		mlen = 0x80;
	}

	if (mlen == 0) {
		mlen = 60;
	}

	CC1200_rfOn();
	CC1200_setReady();
	CC1200_open(ch);
	CC1200_setOutputPower(txLevel);

	uint8 firstByte = 0;
	int tx_cnt = 1;

	int bLoop = 1;

	while (bLoop) {
		for (int i = 0; i < mlen; i++) {
			payload[i] = firstByte + i;
		}

		if (zigMsg == FALSE) {
			printf("[%4d] FirstByte[%02X]", tx_cnt, firstByte);
			for (int i = 0; i < mlen; i++) {
				if (i % 25 == 0) {
					printf("\n   ");
				}
				printf("%02X ", payload[i]);
			}
			printf("\n");
		}

		CC1200_sendMessage2RF(payload, mlen, SHORT_PREAMBLE);

		for (int i = 0; i < 100; i++) {
			MISC_delayMs(30);
			if (TEST_stop() == TRUE) {
				bLoop = 0;
				break;
			}
		}

		firstByte++;

		tx_cnt++;
		if (nMessage && tx_cnt >= nMessage) {
			break;
		}
	}
	CC1200_setRxMode();
	CC1200_worDuty(WOR_SHORT_DUTY);
	CC1200_wor();
}

void TEST_RxData(int ch, BOOL zigMsg)
{
	if (zigMsg) {
		printf("ACK\n");
	} else {
		printf("[Test_RxData] ch:%d\n", ch);
	}

	CC1200_rfOn();
	CC1200_setReady();
	CC1200_setChannel(ch);
	CC1200_setRxMode();
	CC1200_worDuty(WOR_SHORT_DUTY);
	CC1200_wor();

	uint32 rtime;
	uint8 len;
	uint8 rssi;
	uint8 crcOK;

	while (1) {
		// 주의 사항 : 반드시 아래 테스트용 함수는 TEST_stop()보다 먼저 Call되어야 한다.
		// - 테스트용 메세지 확인 함수에서 read pointer를 조작하는 동작이 포함됨.
		// - 다만 RF Rx발생 시 ISR에서 event를 등록, TEST_stop()에서 event를 처리한다.
		// - 위 이유로 TEST_stop()를 먼저 실행시킬 경우 테스트 함수에서 잘못된 read pointer를
		//   참조하게 된다.
		if (CC1200_testGetReceivedData(&rtime, TestRxBuffer, &len, &rssi, &crcOK) == TRUE) {
			if (zigMsg) {
				int errCnt = 0;
				uint8 startPattern = TestRxBuffer[20];

				for (int i = 0; i < len - 24; i++) {
					if (TestRxBuffer[20 + i] != startPattern) {
						errCnt++;
					}
					startPattern++;
				}

				if (errCnt == 0) {
					uint8 rxPower = rssi; //cc1020GetRXpower(rssi, VGA_SETTING);
					printf("trx OK %2d\n", rxPower);
				}
			} else {
				printf("[%08ld] Data Received [%d bytes] rssi[%3d] crc[%d]", rtime,
				       len, rssi, crcOK);
				for (int i = 0; i < len; i++) {
					if (i % 25 == 0) {
						printf("\n   ");
					}
					printf("%02X ", TestRxBuffer[i]);
				}
				printf("\n");
			}
		}

		if (TEST_stop() == TRUE) {
			break;
		}
		MISC_delayMs(30);
	}

	CC1200_setChannel(0);
	CC1200_setRxMode();
	CC1200_worDuty(WOR_SHORT_DUTY);
	CC1200_wor();
}

void TEST_RxMsg(int ch)
{
	uint32 rx_time;
	uint8 rssi;
	uint8 len;
	BOOL crc;

	CC1200_rfOn();
	CC1200_setReady();
	CC1200_setChannel(ch);
	CC1200_setRxMode();
	CC1200_worDuty(WOR_SHORT_DUTY);
	CC1200_wor();

	while (1) {
		// 주의 사항 : 반드시 아래 테스트용 함수는 TEST_stop()보다 먼저 Call되어야 한다.
		// - 테스트용 메세지 확인 함수에서 read pointer를 조작하는 동작이 포함됨.
		// - 다만 RF Rx발생 시 ISR에서 event를 등록, TEST_stop()에서 event를 처리한다.
		// - 위 이유로 TEST_stop()를 먼저 실행시킬 경우 테스트 함수에서 잘못된 read pointer를
		//   참조하게 된다.
		if (CC1200_testGetReceivedData(&rx_time, TestRxBuffer, &len, &rssi, &crc) == TRUE) {
			if (crc == FALSE) {
				printf("CRC Error\n");
			} else {
				displayMessage(rx_time, TestRxBuffer, len, rssi);
			}
		}

		if (TEST_stop() == TRUE) {
			break;
		}
		MISC_delayMs(30);
	}
}

#define RSSI_THRESHOLD 35
void TEST_channelScan(uint32 group)
{
	uint8 index = 0;
	uint8 baseChannel = 5 * ((uint8)group - 1) + 1;

	CC1200_rfOn();
	CC1200_setReady();

	while (1) {
		uint8 ch = baseChannel + index++;
		CC1200_setChannel(ch);
		CC1200_Rx();
		MISC_delayMs(10);
		int8 rssi = CC1200_readRSSI();

		if (ch == baseChannel) {
			printf("\n");
		}
		printf(" %2d:", ch);
		rssi = rssi <= RSSI_THRESHOLD ? 0 : rssi - RSSI_THRESHOLD;
		rssi /= 5;
		for (int i = 0; i < 10; i++) {
			if (rssi > 0) {
				rssi -= 1;
				printf("*");
			} else {
				printf(" ");
			}
		}
		if (index >= 5)
			index = 0;

		if (TEST_stop() == TRUE) {
			break;
		}
	}
}

void TEST_checkRSSI(int ch)
{
	CC1200_rfOn();
	CC1200_setReady();
	CC1200_setChannel(ch);

	while (1) {
		CC1200_Rx();
		MISC_delayMs(10);
		int8 rssi = CC1200_readRSSI();

		printf("RSSI[%03d]", rssi);

		rssi = (rssi <= RSSI_THRESHOLD) ? 0 : rssi - RSSI_THRESHOLD;

		for (int i = 0; i < rssi; i++) {
			printf("*");
		}

		printf("\n");

		MISC_delayMs(25);

		if (TEST_stop() == TRUE) {
			break;
		}
	}
	CC1200_setRxMode();
	CC1200_worDuty(WOR_SHORT_DUTY);
	CC1200_wor();
}

void TEST_checkRxPower(int ch)
{
	int8 rxPower;
	int8 rssi;
	int16 totalRssi;
	uint8 maxPower = 0, minPower = 255;
	int8 maxRssi = -127, minRssi = 127;

	CC1200_rfOn();
	CC1200_setReady();
	CC1200_setChannel(ch);

	while (1) {
		CC1200_Rx();
		MISC_delayMs(10);
		totalRssi = 0;

		maxRssi = -127;
		minRssi = 127;

		for (int i = 0; i < 40; i++) {
			rssi = CC1200_readRSSI();
			totalRssi += rssi;

			if (maxRssi < rssi)
				maxRssi = rssi;

			if (minRssi > rssi)
				minRssi = rssi;

			MISC_delayMs(25);
		}

		rssi = (uint8)(totalRssi / 40);
		minPower = CC1200_getRxPower(minRssi);
		maxPower = CC1200_getRxPower(maxRssi);
		rxPower = CC1200_getRxPower(rssi);

		printf("RSSI : %3d    [%3d <-> %3d ] ", rssi, maxRssi, minRssi);
		printf(" \t RxPower : -%3d dBm    [ -%3d  <->  -%3d ] dBm", rxPower, minPower,
		       maxPower);

		printf("\n");

		if (TEST_stop() == TRUE) {
			break;
		}
	}
	CC1200_setRxMode();
	CC1200_worDuty(WOR_SHORT_DUTY);
	CC1200_wor();
}

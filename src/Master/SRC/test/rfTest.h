#ifndef __RF_TEST_H__
#define __RF_TEST_H__

//=======================================
// AMI Message Type
//=======================================
#define MSG_NODE_JOIN_NOTIFY 0x01
#define MSG_AMI_TIME_SYNC_REQ 0x02
#define MSG_AMI_TIME_SYNC_SET 0x03
#define MSG_AMI_NODE_EVENT_ALARM 0x04
#define MSG_AMI_NODE_STATUS_REQ 0x05 // not use
#define MSG_AMI_NODE_STATUS_REPORT 0x06
#define MSG_AMI_METER_CONF_SET 0x07
#define MSG_AMI_DATA_REPORT 0x08 // not use
#define MSG_PDA_DATA_REQ 0x09
#define MSG_PDA_DATA_REPORT 0x0A // not use
#define MSG_PDA_ONE_DATA_REPORT 0x0C
#define MSG_PDA_DATA_REQ_FINISH 0x0D // not use
#define MSG_PDA_NODE_CONF_SET 0x0E

#define MSG_PDA_NODE_CONF_SUCCESS 0x10
#define MSG_PDA_NODE_SCAN_REQ 0x11 // not use
#define MSG_PDA_NODE_CONF_REQ 0x13
#define MSG_PDA_NODE_CONF_REPORT 0x14
#define MSG_NODE_TIME_REQ 0x18
#define MSG_NODE_TIME_REPORT 0x19
#define MSG_PDA_GROUP_DATA_REPORT 0x1A
#define MSG_PDA_PERIOD_DATA_REPORT 0x1B
#define MSG_PDA_NODE_RXPOWER_REQ 0x1C
#define MSG_PDA_NODE_RXPOWER_REPORT 0x1D
#define MSG_PDA_GROUP_SCAN_REQ 0x1E
#define MSG_PDA_MULTI_DATA_REPORT 0x1F

#define MSG_AMI_NODE_JOIN_ACK 0x22
#define MSG_AMI_NODE_STATUS_ACK 0x24 // not use
#define MSG_AMI_MULTI_DATA_REPORT 0x25
#define MSG_AMI_DATA_ACK 0x26
#define MSG_PDA_PULSE_VALUE_SET 0x27
#define MSG_PDA_PULSE_VALUE_SUCCESS 0x28
#define MSG_AMI_SLAVE_DATA_REPORT 0x29
#define MSG_AMI_NODE_INFO_REPORT 0x2A
#define MSG_AMI_NODE_MULTI_INFO_REPORT 0x2B
#define MSG_AMI_MULTI_METER_REPORT 0x2C
#define MSG_PDA_PERIOD_MULTI_DATA_REPORT 0x2D

#define MSG_AMI_SLAVE_NODE_JOIN_NOTIFY 0x41
#define MSG_AMI_MASTER_SLAVE_METER_REQ 0x42
#define MSG_AMI_MASTER_SLAVE_STATUS_REQ 0x43
#define MSG_AMI_MASTER_SLAVE_STATUS_REPORT 0x44
#define MSG_PDA_SLAVE_CHECK_REQ 0x45
#define MSG_PDA_SLAVE_CHECK_REPORT 0x46
#define MSG_MASTER_SLAVE_CHECK_REQ 0x47
#define MSG_MASTER_SLAVE_CHECK_REPORT 0x48
#define MSG_PDA_MASTER_SLAVE_METER_REQ 0x49
#define MSG_AMI_SLAVE_NODE_EVENT_ALARM 0x4A

#define MSG_NODE_FW_VER_REPORT 0xA0
#define MSG_TEST_NODE_TX_REQ 0xA1 // not use
#define MSG_TEST_NODE_TX_ACK 0xA2 // not use
#define MSG_METER_DATA_ERASE_REQ 0xA3
#define MSG_METER_DATA_ERASE_ACK 0xA4

// messages for Meter Control
#define MSG_PDA_FREQ_BAND_SET 0x39 // pda --> T
#define MSG_PDA_FREQ_BAND_SUCCESS 0x3A // T --> pda

// messages for E-Meter
#define MSG_PDA_E_METER_DATA_REQ 0x51 // pda --> T
#define MSG_PDA_E_METER_DATA_REPORT 0x52 // T --> pda
#define MSG_AMI_E_METER_DATA_REPORT 0x53 // T --> sink

void TEST_readIDs();
void TEST_writeIDs(char *pPanid, char *pNwkid, int slaveID);
void TEST_ContinuousTx(int ch, int txLevel, BOOL zigMsg);
void TEST_TxData(int ch, int txLevel, int mlen, BOOL zigMsg);
void TEST_RxData(int ch, BOOL zigMsg);
void TEST_RxMsg(int ch);
void TEST_channelScan(uint32 group);
void TEST_checkRSSI(int ch);
void TEST_checkRxPower(int ch);

#endif
#ifndef __TEST_H__
#define __TEST_H__

BOOL TEST_stop();
BOOL TEST_checkTestMode();
BOOL TEST_isTestMode();
void TEST_readRTC(int fromJig);
void TEST_setRTC(int fromJig, int year, int mon, int day, int hour, int min, int sec);
void TEST_readFlash(int sector);
void TEST_eraseFlash(int sector);
void TEST_lpm3(int mode);
void TEST_lpm2();
void TEST_reedSensor();
void TEST_readSerialNum();
void TEST_writeSerialNum(char *serialNum);
void TEST_lcd();
void TEST_meterLCD();
void TEST_config();
void TEST_meteringParamer(int ri, int mi);
void TEST_sleepMode(int flag);
void TEST_riCtrlMode(int flag);
void TEST_setMeterType(int type);
void TEST_shortInterval(int flag);
void TEST_debugPrint(int flag);
void TEST_readMeter(int type);

void TEST_pwsn(char *sn, int ri, int mi);
void TEST_pmwsn(char *sn, int ri, int mi, int caliber, int q3, int qt, int q2, int q1, int maker);

void TEST_prsn();
void TEST_pmrsn(BOOL isJigTest);
void TEST_pwmeter(int numMeter, int type1, int port1, int type2, int port2, int type3, int port3);
void TEST_readBattery();
void TEST_pmeter(int numMeter, int type1, int port1, int type2, int port2, int type3, int port3);
void TEST_checkNFC();
void TEST_resetNFC();

void TEST_run();
void TEST_runTasks();
void TEST_readFwVersion();

void TEST_writeMeter(char *valueStr);
#endif

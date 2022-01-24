
#ifndef __BATTERY_H__
#define __BATTERY_H__

#include "common_header.h"

void BATT_init();
int BATT_getLevel();
uint8 BATT_getVoltage(void);
uint8 BATT_getLastVoltage(void);
void BATT_loadLastVoltage(void);

#endif // __BATTERY_H__

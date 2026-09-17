#ifndef STORAGE_DISC_H
#define STORAGE_DISC_H

#include "stm32f1xx_hal.h"

void StorageDisc_Init(void);
void StorageDisc_Stop(void);
int32_t StorageDisc_ReadAngle10(void);
uint8_t StorageDisc_MoveTo(uint16_t degree);
uint8_t StorageDisc_Jog(uint8_t reverse, uint16_t electrical_cycles);
const char *StorageDisc_LastStatus(void);

#endif

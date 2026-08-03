#ifndef MAG_H
#define MAG_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int16_t mag_x, mag_y, mag_z;
} Mag_Data_t;

bool Mag_Init(void);
void Mag_StartAuto(void);
void Mag_StopAuto(void);
bool Mag_Read(Mag_Data_t *pData);

#endif // MAG_H
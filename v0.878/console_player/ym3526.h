#ifndef YM3526_H
#define YM3526_H

#include <stdint.h>

void ym3526_init(uint8_t slot);
void ym3526_mute(uint8_t slot);
void ym3526_write_reg(uint8_t slot, uint8_t reg, uint8_t data);

#endif // YM3526_H

#ifndef YM3812_H
#define YM3812_H

#include <stdint.h>

void ym3812_init(uint8_t slot);
void ym3812_mute(uint8_t slot);
void ym3812_write_reg(uint8_t slot, uint8_t reg, uint8_t data);

#endif // YM3812_H

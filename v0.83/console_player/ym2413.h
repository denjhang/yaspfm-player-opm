#ifndef YM2413_H
#define YM2413_H

#include <stdint.h>

void ym2413_write_reg(uint8_t slot, uint8_t addr, uint8_t data);
void ym2413_init(uint8_t slot);
void ym2413_mute(uint8_t slot);

#endif // YM2413_H

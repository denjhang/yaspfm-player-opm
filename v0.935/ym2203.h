#ifndef YM2203_H
#define YM2203_H

#include <stdint.h>

void ym2203_write_reg(uint8_t slot, uint8_t addr, uint8_t data);
void ym2203_init(uint8_t slot);
void ym2203_mute(uint8_t slot);

#endif // YM2203_H

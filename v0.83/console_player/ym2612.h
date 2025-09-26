#ifndef YM2612_H
#define YM2612_H

#include <stdint.h>

void ym2612_write_reg(uint8_t slot, uint8_t port, uint8_t addr, uint8_t data);
void ym2612_init(uint8_t slot);
void ym2612_mute(uint8_t slot);

#endif // YM2612_H

#ifndef YM2151_H
#define YM2151_H

#include <stdint.h>

void ym2151_write_reg(uint8_t slot, uint8_t addr, uint8_t data);
void ym2151_mute(uint8_t slot);
void ym2151_init(uint8_t slot);

#endif /* YM2151_H */

#ifndef AY8910_H
#define AY8910_H

#include <stdint.h>

void ay8910_init(uint8_t slot);
void ay8910_mute(uint8_t slot);
void ay8910_write_reg(uint8_t slot, uint8_t reg, uint8_t data);

#endif // AY8910_H

#ifndef YM2608_H
#define YM2608_H

#include <stdint.h>

void ym2608_write_reg(uint8_t slot, uint8_t port, uint8_t addr, uint8_t data);
void ym2608_init(uint8_t slot);
void ym2608_mute(uint8_t slot);

#endif // YM2608_H
